# Batch 0138 — lock-order certification

## Scope

Complete SEC-013, SEC-014, and SCH-015 for all currently active blocking kernel
spinlocks.

## Implementation

- Defined a single outer-to-inner rank hierarchy.
- Instrumented endpoint, IPC lifecycle, capability, memory, and object locks.
- Enforced equal-rank address ordering and strict reverse release.
- Compiled checker state out of release builds.
- Added certification checks for valid nesting, recursive rejection,
  equal-rank ordering, and zero observed violations.

## Evidence

- The four-CPU certification suite completes with zero lock-order violations and
  zero acceptance failures.
- The release image builds and passes ELF policy checks.
- `git diff --check` reports no whitespace errors.
