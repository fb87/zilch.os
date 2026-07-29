# Batch 0170: bounded capability completion gate

## Scope

This batch closes the kernel 1.0 capability system for its explicit fixed
capacity contract. It does not claim dynamically growing CSpaces or an
unbounded derivation graph.

## Production contract

- At most 32 registered CSpaces, each with guarded two-level 4×64 selectors.
- At most 4,095 usable generation-tagged derivation records and depth 64.
- Copy, mint, move, delete, revoke, IPC transfer, mapping authority, and
  process retirement serialize through the capability authority transaction.
- Descendant revoke scans the bounded registry in one two-phase transaction.
- Calls and replies transfer at most four receiver-placed capabilities, with
  complete rollback on duplicate, occupied, or failed destinations.
- Capacity or generation exhaustion fails closed without overwriting live
  authority.

## New acceptance evidence

`capability::database_valid()` freezes mutation and checks the complete live
database: registry indices, guards, allocation hints, occupied live slots,
unique active derivations, generation encoding, exact object-matched ancestry,
depth roots, and object resolution.

The userspace `capability_completion_gate` aggregates the existing attenuation
and recursive-revoke loop, batched transfer and rollback, cross-CPU
transfer/revoke, lookup/destroy, mapping-authority revoke, memory-server
transfer, dynamic IPC, four-CPU fuzz, complete teardown, and generation reuse
workloads.

## Verification

- `make format-check abi-check boundary-check`
- `make BUILD_VARIANT=certification run`
- `make production-gate`
- AMD64 release build, ELF, section-permission, and stack-usage checks

The readiness checklist is marked complete only after these gates pass.
