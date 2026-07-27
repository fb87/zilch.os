# Batch 0151: per-CPU tickless deadlines

This batch closes TIM-002, TIM-003, TIM-004, and SCH-019 as one shared-timebase
correctness change.

- Program each ARM64 virtual timer from an absolute per-CPU scheduler deadline.
- Drive idle programming from the earliest IPC/fault timeout queue entry.
- Preserve the active one-tick scheduling quantum for budget and preemption.
- Advance logical time by the actual one-shot delta.
- Validate counter frequency and clamp hardware intervals.
- Saturate scheduling, IPC, and pager deadline arithmetic on overflow.
- Certify the live timebase alongside timeout ordering and full acceptance.

SCH-010 remains open: fixed-period budget replenishment is not claimed as
sporadic-server conformance.
