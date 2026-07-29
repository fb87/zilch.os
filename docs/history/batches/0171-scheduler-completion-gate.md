# Batch 0171: bounded scheduler completion gate

## Production contract

The 1.0 scheduler supports four immutable online CPUs, ten user-thread slots,
fixed CPU affinity with quiescent migration, 256 priorities, eight donation
levels, sixteen replenishments per scheduling context, and ten timeout entries
per CPU. Capacity exhaustion and invalid configuration fail closed.

Scheduling reconfiguration is accepted only while the target is suspended and
has no live reply or donation state. Priority, budget, period, affinity, and
replenishment state then commit under one lifecycle lock.

## Completion evidence

- `rt_logical_time_soak` advances six logical hours and 21,600 one-second
  periods with exact throttling and replenishment.
- `scheduler_database_invariants` checks context affinity, budgets, donation
  depth, replenishment ordering/accounting, and per-CPU timeout ordering.
- `scheduler_latency_bounds` requires nonzero IRQ-disabled, IRQ,
  timer-preemption, cross-CPU-wake, and IPC samples and enforces the QEMU
  profile's ten-millisecond limit.
- `scheduler_completion_gate` aggregates configuration/migration, donation and
  lifecycle races, pager and memory services, four-CPU fuzz, teardown, and
  generation reuse.

The virtual-platform latency bound is distinct from the checklist's
real-hardware qualification gate.

## Verification

- `make format-check abi-check boundary-check`
- `make BUILD_VARIANT=certification run`
- `make production-gate`
- AMD64 compile-only release ELF, section-permission, and stack-usage checks
