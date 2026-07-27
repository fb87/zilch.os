# Scheduling-context replenishment and lock evidence

Each context has a fixed 64-entry replenishment queue. Charging `n` units at
logical time `t` inserts one ordered record for `n` units at `t + period`.
When records become due, their exact amounts are returned and the context
becomes eligible again once its consumed amount falls below budget. A full
queue fails closed by throttling instead of silently losing accounting.

Configuration rejects zero values, budget greater than period, and any period
whose absolute deadline would overflow the logical timebase. Donation remains
separate: donated budget is consumed through the existing authority chain and
does not create an unowned replenishment record.

Every ranked kernel lock records its acquisition counter and updates a
generation-safe maximum on release. Certification reports the maximum hold
duration in architectural timer ticks and rejects lock-order violations.
IRQ-disabled-section timing is tracked separately by SCH-017 and is not
claimed by this instrumentation.
