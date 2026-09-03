# Module: user_access

## Purpose

amd64's stand-in for bounds-checked user-memory access. amd64 has a real
per-process page table (`space::address_space::pt`) but no SMAP-safe
unprivileged load/store sequence wired up yet, and no real syscall entry
(Phase 7) that would ever reach this path -- `arch::syscall::is_user_syscall()`
is unconditionally `false` on this platform today. Selected via this
directory's entry on the kernel arch include path so that
`sys::kernel::user_access` (a thin re-export) needs no
`#if defined(__x86_64__)` guard.

## Responsibilities

- Provide the same three-function interface as arm64's implementation
  (`valid_range`, `copy_from_user`, `copy_to_user`), each honestly
  reporting failure/`unsupported` rather than guessing at an unverified
  bounds-check or copy sequence.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Must not silently claim success: any future real implementation needs a
  genuine SMAP-safe (or SMAP-absent-verified) copy path before these
  return anything but failure.

## Verification

Unreachable today: this platform's only caller
(`sys::kernel::syscall::capture_transfer`) lives in a translation unit
(`src/kernel/include/sys/kernel/syscall/ipc.hh`) not yet included from
amd64's `arch.cc`.
