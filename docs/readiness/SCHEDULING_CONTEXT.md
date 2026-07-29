# Scheduling-context budget and lock evidence

Each context has a fixed sixteen-entry replenishment queue. Charging `n` units
at logical time `t` inserts an ordered record for `n` units at `t + period`.
When records become due, their exact amounts are returned and the context
becomes eligible once consumption falls below its budget.

Equal-deadline records are merged. If distinct deadlines fill the queue, a new
record is coalesced into the latest entry and its deadline is delayed to the
later deadline. This can defer budget but cannot return it early, exceed the
configured bandwidth, drop accounting, or leave a throttled context without a
future replenishment. Configuration rejects zero values, budget greater than
period, and absolute-deadline overflow. Donation remains separately accounted
through the existing authority chain.

Every ranked kernel lock records its acquisition counter and updates a
generation-safe maximum on release. Scoped logging and scheduler timeout-queue
sections also retain per-CPU IRQ-disabled sample counts and maximum duration.
IRQ service, timer preemption, cross-CPU wake, and IPC service use the same
telemetry foundation. The QEMU certification profile enforces a deliberately
loose ten-millisecond upper bound for IRQ-disabled, IRQ-service,
preemption-service, cross-CPU-wake, and IPC-service maxima. Hardware-specific
qualification remains part of the separate real-hardware release gate.

Scheduler charges and IPC donation use the current per-CPU logical timer.
Donated unconsumed budget receives a future replenishment record before the
donor is throttled. Certification covers staggered slice return,
exhausted-context throttling, absolute-deadline overflow rejection, full-queue
coalescing, donation, eventual recovery, pager liveness, and complete
root-only acceptance.

Scheduling reconfiguration requires a suspended thread with no reply or
donation authority. ABI argument five is an optional affinity encoding: zero
preserves the current CPU and values one through four select CPUs zero through
three. Priority, budget, period, affinity, and replenishment state commit under
the IPC lifecycle lock.

Certification additionally advances 21,600 one-second periods, representing
six logical hours, and requires exact throttle and replenishment behavior with
no accounting debt or deadline violation. Final validation checks every live
context and every per-CPU timeout queue.
