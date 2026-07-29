# Batch 0159: progress-safe sporadic replenishment

This batch restores per-slice sporadic-server accounting after the batch 0152
experiment was reverted for fault-service liveness.

- Each charged slice receives an absolute replenishment deadline one period
  after consumption.
- A sixteen-entry ordered queue merges equal deadlines.
- Queue overflow coalesces into the latest entry. The resulting delay is
  bandwidth-safe and preserves an eventual replenishment, avoiding the earlier
  permanent-throttle failure mode.
- Certification verifies staggered return, throttling, deadline overflow
  rejection, full-queue coalescing, and eventual recovery.
- Production scheduler charging and IPC donation use the current per-CPU
  logical timer, including a replenishment record for transferred budget.
- Pager invalid-reply, nested-fault, pager-timeout, timeout-queue, and
  fault-reply checks pass after the scheduler test.
- EL2 diagnostic hexadecimal output uses register-only digit conversion so
  verbose guest-state reporting cannot fault on a lookup-table data access
  while guest ownership is active.

SCH-009 and SCH-010 are complete. The full ARM64 certification run reaches
root-only acceptance with zero failures and passing transport.
