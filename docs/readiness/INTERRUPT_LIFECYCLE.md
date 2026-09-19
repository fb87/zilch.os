# Physical interrupt lifecycle

Status: complete for the QEMU ARM64 1.0 platform profile

## Ownership

Each physical IRQ below the GIC spurious range has at most one registered
interrupt object. Registration fails if the line is already owned. The object
is generation checked through the normal object table and is controlled through
capability rights:

- `control` binds a notification;
- `write` acknowledges and re-enables delivery;
- `grant` permits rights-attenuated delegation to another CSpace.

The kernel does not support shared physical lines in 1.0. A userspace driver
service may own one physical line and demultiplex device-specific sources to
clients.

## Delivery state machine

Registration configures edge or level behavior and leaves the line masked.
Binding a valid notification clears storm state and unmasks it. On delivery the
kernel atomically marks the object active, masks the line, increments its
delivery counter, and signals a badge derived from the IRQ number.

The ARM64 GIC uses EOImode 1. Exception return writes `ICC_EOIR1_EL1` to drop
priority but leaves a userspace-owned line active. An authorized acknowledge
requires the object to be active, writes `ICC_DIR_EL1`, increments the
acknowledge counter, and unmasks the line unless storm containment is active.
Duplicate acknowledge returns `not_found`.

Timer and kernel IPI lines are reserved. Their handlers perform priority drop
and deactivate in the exception path and cannot be redirected through a
userspace IRQ object.

An acknowledge also re-routes the line to the CPU that performed it, so a
line follows the thread that services it rather than staying on whichever CPU
happened to take the first delivery.

## Ownership teardown

Three paths end or transfer ownership, and all of them sever authority at the
controller rather than only removing a name.

`release_ownership` masks the line, drops any in-flight delivery (deactivating
at the controller if the object was still active), clears the bound
notification, and resets storm state. It is idempotent and safe on a line that
was never bound: every step is either a store to a known value or a controller
operation that tolerates repetition.

Revoking an interrupt capability runs that same path, through a hook the
capability layer calls on revocation. The hook is shared with device memory:
the same callback tears down a revoked device frame's mappings, so revoking
either half of a device assignment — its interrupt or its MMIO — withdraws
real access rather than just a name. This is load-bearing rather than tidy:
revocation previously removed only the CSpace entry, leaving the object bound
to the old owner's notification and still unmasked, so the device kept firing
into a driver that no longer owned the line. The name was revoked and the
authority was not, which is not revocation. Root re-delegating the line calls
`bind` again, which re-arms it.

`bind` refuses with `busy` when a delivery is still outstanding, because the
current owner owes an acknowledge and rebinding under it would strand that --
with one exception. If the notification the line was bound to no longer
resolves, the owning task was destroyed with a delivery in flight and nothing
will ever acknowledge it; `active` would stay set for the remaining uptime and
every future bind would fail. Binding therefore takes the line over in that
case, deactivating at the controller first so the new owner starts from a clean
GIC state. Without this a driver was unrestartable precisely when it most
needed restarting -- after crashing while servicing its own interrupt.

## Storm containment and diagnostics

Each object maintains a delivery window intended to be 100 ticks wide, with
more than 64 observations in one window setting `stormed`, masking the line and
suppressing further notification. Recovery is either an explicit `bind`, which
requires control authority, or a bounded sweep driven from the timer on CPU 0
that clears storm state once the window has elapsed and unmasks any line whose
owner does not still owe an acknowledge.

**This containment is not currently in force.** The window is stamped with
`platform::timer::ticks()`, which is per-CPU, and the two ends of the
comparison read different CPUs' counters: deliveries stamp the window from
whichever CPU took the interrupt, while the recovery sweep always reads CPU 0.
Those counters advance independently, so the elapsed-time subtraction compares
unrelated clocks and underflows whenever CPU 0 lags, which resets the window on
essentially every delivery and keeps the count from ever approaching the
threshold. Making the window real by pinning both ends to one clock was
measured and regresses certification's latency bounds -- the threshold is far
below legitimate device interrupt rates, and reading one CPU's counter on every
dispatch puts cross-CPU cache-line traffic on the hot path. See
PRODUCTION_READINESS_CHECKLIST entry 0158 for the measurements and what a real
fix requires. Treat storm containment as unimplemented until that closes.

Objects retain delivered, acknowledged, and suppressed counters plus active,
masked, stormed, window-start, and window-count state. Counter updates and state
transitions use atomic publication because delivery and userspace acknowledge
may execute on different CPUs.

## Platform scope

The fixed QEMU profile exposes userspace-assignable SPIs 32 through 1019.
Private interrupts, the virtual timer, and kernel IPIs are rejected by the
public interrupt-object registry. Certification uses exclusive edge IRQ 40 and
level IRQ 41, verifies duplicate/reserved rejection, delegates attenuated
authority through a guarded CSpace, and revokes it.

Real-hardware discovery, polarity, routing, and redelivery qualification remain
part of the independent real-hardware release gate and are not claimed by the
virtual-platform completion gate.
