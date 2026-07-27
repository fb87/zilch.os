# Batch 0142: guarded scalable CSpaces

## Outcome

The production CSpace moved from one 64-slot linear array to a guarded
two-level radix containing 256 slots. Bitmap allocation, guard validation, and
address-ordered global revoke locking are part of the production mechanism.

## Evidence

- `guarded_cspace_scale`: 193 live derived capabilities span all four radix
  leaves; correct-guard lookup succeeds, wrong-guard lookup fails, bulk revoke
  removes 193 descendants, and every occupancy bitmap returns to zero.
- `cross_cspace_transfer_fuzz`: 4,096 generated copy, lookup, negative guard,
  delete, and slot-reuse operations pass.
- `capability_transfer_revoke_race`: the existing four-CPU transfer-versus-
  revoke invariant remains part of certification.
- Full ARM64 certification, ARM64/AMD64 release compilation, ABI checks, header
  checks, source-boundary checks, and diff checks are required before commit.

## Scope

This batch completes CAP-003, CAP-004, CAP-005, and CAP-020. It does not claim
completion of the globally bounded derivation table, restartable revoke,
multi-capability IPC transfer, or the capability completion gate.
