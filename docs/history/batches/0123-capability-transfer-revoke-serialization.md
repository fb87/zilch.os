# Batch 0123: capability transfer/revoke serialization

## Goal

Prevent an IPC capability transfer from publishing a new derivation after a
concurrent revoke has completed its descendant scan.

## Implementation

- IPC transfer now acquires the global capability authority lock around mint.
- The lock is the same transaction boundary used by control-path copy, mint,
  move, delete, revoke, and mapping authority operations.
- Public descendant and object-reference revoke operations acquire that lock
  internally; callers already inside an authority transaction use explicit
  locked primitives, preventing both omitted locking and recursive deadlock.
- Certification roles `0x115` and `0x116` use the pager-client image; only the
  server role receives endpoint read authority.

## Certification

`capability_transfer_revoke_race` creates a notification authority, delegates a
grant-bearing child to a sender on CPU 2, and races its IPC transfer to a
receiver on CPU 1 against root revocation on CPU 0.

Both legal linearizations must end identically:

- transfer first: revoke observes and removes the receiver descendant;
- revoke first: transfer cannot derive from the invalid source.

After revoke returns, deleting the receiver slot must therefore return
`not_found`. The test then destroys both processes and the notification, and
the remaining pager, memory, fuzz, and object-reuse suites validate cleanup.

The larger certification init image also exposed two fixture assumptions. The
bootstrap pager test now explicitly loads the pager-client image, and mapping
database scratch pages no longer overlap the expanded init executable.

## Evidence

- `make BUILD_VARIANT=certification format clean run`
- `[TEST] name=capability_transfer_revoke_race result=PASS`
- `[ACCEPTANCE] ... result=PASS failures=0`
