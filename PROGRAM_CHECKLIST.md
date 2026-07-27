# Zilch program checklist

The authoritative requirement tracker is
[`PRODUCTION_READINESS_CHECKLIST.md`](PRODUCTION_READINESS_CHECKLIST.md).
It uses stable requirement IDs and the strict completion rules adopted by the
production-readiness review.

The architectural gap analysis is retained in
[`ARCHITECTURE_ALIGNMENT_REVIEW.md`](ARCHITECTURE_ALIGNMENT_REVIEW.md).
That review was written against an earlier baseline; the reconciliation notes
in the readiness checklist describe which foundations have advanced since then
without overstating production completion.

## Status at patch 0075 alignment

Zilch is an advanced production-development baseline, not a production-ready
kernel or hypervisor.

Verified foundations include ARM64/QEMU boot, MMU, exceptions, GICv3, timer,
four-CPU bring-up, a PL3 root task, bounded generation-safe kernel objects,
capability-checked invocation, allocator-backed memory objects, process-bundle
lifecycle, fault IPC, independently linked pager services, and real
single-vCPU EL2/EL1/EL0 execution.

The following remain release-blocking:

1. Complete capability derivation/revocation concurrency and scalable CSpaces.
2. Complete IPC timeout, cancellation, scheduling-context donation, and
   capability-transfer semantics.
3. Full physical-memory discovery/delegation, scalable mapping database, and
   memory-pressure behavior.
4. Production RT scheduler evidence and RT-safe logging.
5. A real userspace control-plane service graph.
6. General earlyfs ELF loading and path-based process creation.
7. Userspace IRQ/device management and console ownership.
8. Real concurrent multi-vCPU/multi-VM EL2 execution rather than model suites.
9. SMMU/device assignment, hardening, fault injection, soak, CI, and retained
   release evidence.

## Immediate aligned execution order

### Alignment A — product/test separation and truthful claims

- Finish PRD-003, PRD-005, PRD-007 through PRD-018.
- Rename model-only hypervisor profiles as verification suites.
- Split production hypervisor mechanisms from test orchestration.
- Populate requirement-to-code-to-test evidence links.

### Kernel foundations B

- Complete capability derivation/revoke races and scalable lookup.
- Complete IPC cancellation, timeout, reply, transfer, and scheduling-context
  donation semantics.
- Complete physical-resource delegation and mapping lifetime management.
- Make the RT scheduler and deferred logging production mechanisms.

### Userspace control plane C

- General earlyfs ELF64 loader.
- Path-based process manager and root resource server.
- Production memory server/pager policy, including denial, death, pressure, and
  concurrent clients.
- IRQ/device manager, userspace UART/console, supervisor, and VMM/domain manager.

### Real hypervisor execution D

- Four physical CPUs independently run four guest vCPUs through EL2.
- Guest-side SMP/IPI/timer rendezvous using real guest instructions.
- Real preemption, save, migration, re-entry, and concurrent two-VM execution.
- Production virtual GIC/timer and race-safe teardown.

### Certification E

- SMMU and device-assignment isolation.
- Security hardening and threat-model review.
- Failure injection, race fuzzing, latency limits, reproducible builds, and
  long-duration soak on QEMU and real ARM64 hardware.

## Next code batch

Patch **0077** should implement the general earlyfs ELF64 loader. Patch 0076 is
reserved for this review/readiness alignment update.

Required initial runtime records:

```text
[TEST] name=earlyfs_elf_load result=PASS
[TEST] name=elf_bss_zeroing result=PASS
[TEST] name=elf_segment_permissions result=PASS
[TEST] name=elf_loader_rollback result=PASS
[TEST] name=elf_loader_negative_cases result=PASS
```

These records establish loader behavior only; they do not complete USR-013
through USR-018 until path-based userspace process management, startup state,
failure reporting, and the associated evidence gates are satisfied.
