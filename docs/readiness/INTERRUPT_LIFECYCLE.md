# Physical interrupt lifecycle

Status: production kernel mechanism; platform discovery integration remains open

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

## Remaining integration

Production platform description must discover external IRQ resources, create
their objects, and delegate them through a userspace device manager. Real
hardware edge and level devices must validate polarity, routing, and
redelivery. Those gaps keep IRQ-002, IRQ-003, and IRQ-005 in progress.
