# Zilch production-kernel cumulative batch status

> **Historical status notice:** This file records the state of its named batch. The current program status, completed follow-on work, remaining requirements, and delivery order are maintained in [`docs/roadmap/PROGRAM_CHECKLIST.md`](../../roadmap/PROGRAM_CHECKLIST.md).

This tree continues the production-alignment work from batch 0062. It is not a production-complete kernel and must not be labeled as such.

## Implemented in this increment

### Capability core

- 64-slot registered CSpaces with explicit allocation failure.
- Generation-safe object references retained.
- Persistent derivation records with bounded depth.
- Copy with rights attenuation and parent tracking.
- Mint with rights attenuation and badges.
- Atomic cross-CSpace move.
- Single-capability delete.
- Recursive descendant revoke across all registered CSpaces.
- Full reference revoke across registered CSpaces.
- Certification test covering mint, badge-bearing derivation, and descendant revoke while preserving the ancestor.
- Native ABI operation for capability mint.

Implementation status: complete for the current bounded kernel configuration.
Verification status: compile and certification-build validation complete; runtime SMP revoke/transfer race verification pending.

### Scheduler core

- Base and effective priorities.
- Priority-ordered runnable selection with deterministic round-robin tie order.
- Budget and period validation.
- Budget charging and throttling.
- Periodic replenishment.
- Bounded priority donation and explicit donation rollback.
- Scheduler eligibility integrated into the user-thread selection path.

Implementation status: core mechanism implemented.
Verification status: compile and release-gate validation complete; runtime latency, deadline, inversion, and migration stress verification pending.

## Explicitly still open

The following remain mandatory before a production-ready kernel claim:

- Concurrent revoke-versus-lookup/IPC quiescence and object reference accounting.
- IPC capability transfer rollback, reply objects, timeout/cancellation, and scheduling-context donation.
- Dynamic physical-memory discovery/delegation and removal of fixed frame fixtures.
- Mapping database, multiple mappings, ASID rollover, and complete SMP shootdown protocol.
- Userspace pager and complete fault-reply policy.
- Targeted cross-CPU preemption, timeout queues, and measured RT latency limits.
- IRQ capability ownership and userspace IRQ broker.
- Product userspace service graph.
- Hardening, fault injection, real-hardware coverage, and soak evidence.

No item above is considered complete merely because this tree compiles.

## IPC production increment

Implemented, verification pending:

- one-shot reply tokens with caller-generation validation;
- priority donation and rollback on reply, cancellation, and timeout;
- single-capability transfer with rights attenuation and badge minting;
- timer-driven bounded IPC timeout handling;
- privileged cancellation and endpoint queue cleanup;
- ARM64 x6/x7 IPC descriptor ABI and userspace invocation wrapper.

Still open:

- object-table reply objects;
- multi-capability transfer;
- scalable timeout queues;
- budget donation;
- race/fault-injection/runtime evidence.
