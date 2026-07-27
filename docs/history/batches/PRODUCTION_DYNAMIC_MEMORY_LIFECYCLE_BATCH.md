# Batch 0100 — Dynamic physical-memory lifecycle foundation

This batch strengthens the bounded allocator-backed memory subsystem while preserving the production-readiness rule that static object pools do not constitute a complete resource model.

## Delivered

- SMP-safe page allocation/release lock.
- Explicit allocatable physical-region inventory for QEMU ARM64.
- Owner-tagged frame and page-table objects.
- Zero-on-allocation and zero-on-release.
- Address-space teardown of all tracked reverse mappings.
- PL3 lifecycle test for eight frames and four page tables with accounting restoration.

## Remaining

- Device-tree/firmware region ingestion and discontiguous RAM.
- Untyped/retype or equivalent root-resource delegation.
- Scalable frame/page-table metadata.
- Full exhaustion, fragmentation, pressure, revoke-race, and fault-injection evidence.
