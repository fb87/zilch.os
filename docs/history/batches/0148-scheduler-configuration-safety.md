# Batch 0148 — Scheduler configuration safety

- Route the public scheduling configuration syscall through the production
  scheduling-context validator.
- Reject priorities wider than the eight-bit ABI value before narrowing.
- Reject zero budgets, zero periods, and budgets greater than their periods.
- Require a suspended, quiescent target with no reply authority or active
  scheduling-context donation.
- Preserve the target's pinned CPU affinity and derive the next replenishment
  deadline from that CPU's current timer.
- Certify live-target rejection, malformed configurations, valid suspended
  configuration, teardown, and reuse through the real control path.
