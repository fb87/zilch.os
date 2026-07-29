# Batch 0160: kernel real-time latency telemetry

This batch adds bounded timing evidence for scoped IRQ-masked kernel critical
sections.

- `interrupt::timing` wraps architectural IRQ save/disable and restore.
- Per-CPU counters retain sample totals and maximum elapsed architectural
  timer ticks.
- Serialized logging and scheduler timeout-queue mutation use the wrapper.
- IRQ handler service, timer preemption service, cross-CPU wake
  request-to-reschedule-IPI receipt, and IPC syscall service share the same
  per-CPU maximum and sample-count mechanism.
- Complete QEMU ARM64 runs emit observations for all five classes.
- Repeated identical runs exposed host-scheduling outliers from below one
  millisecond to more than twenty milliseconds. QEMU reference values are
  therefore diagnostic and do not gate functional acceptance.

SCH-017 and SCH-020 through SCH-023 are in progress. Their production
instrumentation exists; stable limits and retained real-hardware evidence
remain required for completion.
