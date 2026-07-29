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
generation-safe maximum on release. Certification reports the maximum hold
duration in architectural timer ticks and rejects lock-order violations.
IRQ-disabled-section timing remains SCH-017 work. Hardware-specific latency
targets remain outside this bounded evidence.

Scheduler charges and IPC donation use the current per-CPU logical timer.
Donated unconsumed budget receives a future replenishment record before the
donor is throttled. Certification covers staggered slice return,
exhausted-context throttling, absolute-deadline overflow rejection, full-queue
coalescing, donation, eventual recovery, pager liveness, and complete
root-only acceptance.
