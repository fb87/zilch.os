# Module: user_access

## Purpose

arm64's implementation of bounds-checked user-memory access: validating
that a `[address, address+size)` range belongs to the calling thread's
user mapping with the requested permission, and copying bytes across the
kernel/user boundary via unprivileged loads/stores. Selected via this
directory's entry on the kernel arch include path so that
`sys::kernel::user_access` (a thin re-export) needs no
`#if defined(__aarch64__)` guard.

## Responsibilities

- `valid_range`: walks the thread's L3 page table entries covering the
  range, checking the PRESENT bit and AP (access permission) encoding.
- `copy_from_user`/`copy_to_user`: byte-at-a-time copy using `LDTRB`/
  `STTRB` -- ARM's unprivileged-load/store-byte instructions, which fault
  exactly as EL0 would, so a malicious or stale mapping cannot be used to
  read/write kernel-only memory even if a bounds-check bug let a bad
  address through.
- A speculation barrier after a successful `valid_range` check, to close
  the classic Spectre-v1 bounds-check-bypass window before the copy loop
  runs.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Every copy is preceded by `valid_range`; never call `LDTRB`/`STTRB`
  against an address that has not just been validated against the calling
  thread's own mapping.

## Verification

Exercised via `sys::kernel::syscall::capture_transfer`'s out-of-line
capability-transfer-batch path (`src/kernel/include/sys/kernel/syscall/ipc.hh`).
