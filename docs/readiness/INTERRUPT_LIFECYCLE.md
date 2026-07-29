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

## Storm containment and diagnostics

Each object maintains a 100-tick delivery window. More than 64 observations in
one window sets `stormed`, masks the line, and suppresses further notification.
Binding is the explicit recovery action and requires control authority.

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
