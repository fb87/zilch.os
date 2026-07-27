# Batch 0152: sporadic budgets and lock evidence

This batch combines scheduler budget correctness with its first production
timing evidence.

- Replace fixed whole-budget replenishment with bounded, individually timed
  sporadic-server slices.
- Reject replenishment deadlines that overflow the logical timebase.
- Add certification coverage for staggered slices, throttling, due return,
  and overflow rejection.
- Measure ranked lock hold duration using the architectural counter and reject
  lock-order violations during certification.
- Keep IRQ-disabled-section measurement (SCH-017) explicitly open.
