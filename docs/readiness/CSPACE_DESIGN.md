# Capability-space design

Status: implemented ARM64 kernel contract

## Selector geometry

A CSpace contains a two-level radix with four root entries and 64 slots per
leaf. The low eight selector bits identify the radix path: bits 7–6 select the
leaf and bits 5–0 select its slot. Bits 15–8 carry the CSpace guard. Higher
selector bits must be zero.

The bootstrap CSpaces retain guard zero, so every frozen ABI selector keeps its
existing numeric value. A nonzero guard changes authority naming without
changing the object or capability record. Guard changes are rejected while any
slot is occupied, preventing live selectors from being silently reinterpreted.

## Lookup and mutation

All install, lookup, derive, mint, move, delete, and revoke entry points resolve
selectors through the same guard and radix helpers while holding the existing
capability-authority and CSpace locks. A guard mismatch fails before a slot is
read. Object-generation and derivation validation still occur after path
resolution.

Each leaf owns a 64-bit occupancy bitmap. Allocation begins at a rotating hint,
checks at most 256 bits, reserves the selected bit under the CSpace lock, and
returns a selector carrying the current guard. Removal, revoke, and move clear
the source occupancy bit as part of the locked slot mutation.

## Concurrency

Two-CSpace operations lock CSpaces in increasing address order. Global
descendant revoke first sorts registered CSpaces by address, locks in ascending
order, performs its two-phase mark/remove transaction, and unlocks in reverse
order. This keeps equal-rank locking consistent even when registration order
differs from storage order.

## Bounds and remaining work

The 1.0 CSpace capacity is 256 slots. This is an explicit bounded kernel
contract, not a test-only pool. The derivation table remains a separate bounded
global structure; scalable/restartable derivation traversal is tracked by
CAP-008 and CAP-013. Multi-capability IPC transfer remains tracked by
CAP-017 through CAP-019.

Certification fills 193 slots across all four leaves, rejects a wrong guard,
bulk-revokes every descendant, verifies bitmap recovery, and then executes
4,096 generated cross-CSpace copy/lookup/delete operations. The existing
cross-CPU transfer-versus-revoke suite continues to verify its post-revoke
invariant.
