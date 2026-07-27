# Batch 0133: failure-safe emergency diagnostics

## Targets

- Complete SEC-020 per-CPU emergency logging.
- Complete SEC-021 postmortem crash-record preservation.
- Complete OBS-002 IRQ/exception-safe logging.
- Complete OBS-004 per-CPU trace buffers.
- Advance OBS-003 with deferred contention records.

## Implementation

Each CPU owns a lock-free ring of 32 fixed machine-readable records. Producers reserve monotonically increasing sequence numbers, populate a slot, and publish it with a release store. Exception entry records EL, vector, syndrome, and PC before object or scheduler dispatch.

`printk` retains local IRQ masking and record serialization, but acquisition is bounded. If another CPU—or interrupted code on the same CPU—owns the console lock, the caller records contention in its emergency ring and returns instead of spinning indefinitely.

Fatal exceptions and stack-canary failures write a checksummed crash record containing CPU, EL, vector, ESR, FAR, and PC. The linker places this record in a dedicated `.noinit` page after BSS, so normal bootstrap clearing does not destroy prior postmortem state.

## Evidence

Bootstrap certification publishes and reads back a CPU-local record using release/acquire ordering. The clean four-CPU certification run completes real IRQ, fault, SMP race, and root-only acceptance suites with zero failures. The linked ELF contains a distinct NOBITS `.noinit` section outside `__bss_start..__bss_end`.
