# Batch 0173: security and hardening completion gate

## Architectural hardening

The host enables SCTLR WXN in addition to MMU, cache, and instruction-cache
controls. Final acceptance reads back exact MAIR_EL1 and TCR_EL1 constants,
requires SCTLR M/C/I/WXN, rejects big-endian EL1/EL0 state, verifies PAN/UAO,
walks kernel permissions and stack guards, and validates every online CPU's
mitigation inventory.

## Concurrency and failure containment

Security acceptance composes every final subsystem database: objects,
capabilities, mappings, processes, scheduling contexts/timeouts, endpoints,
notifications, interrupts, timers, physical memory, and platform inventory.
The emergency ring and checksummed crash record must remain valid after the
complete workload.

`security_hardening_gate` aggregates capability attenuation, hypervisor
isolation, IPC lifecycle, transfer/revoke, lookup/destroy, pager recovery,
memory-server transport, mapping-authority revoke, rollback, four-CPU fuzz,
teardown, and generation reuse.

The QEMU profile has no watchdog. External machine control plus persistent
emergency/crash state is the explicit failure policy. PAuth/BTI and real
hardware/firmware qualification remain separate.

## Verification

- `make format-check abi-check boundary-check`
- `make BUILD_VARIANT=certification run`
- `make production-gate`
- AMD64 compile-only release ELF, section-permission, and stack-usage checks
