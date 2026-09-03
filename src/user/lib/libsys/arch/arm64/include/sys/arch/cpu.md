# Module: cpu

## Purpose

arm64's userspace-safe CPU helper: a single EL0-legal spin-wait hint
(`wfe`) used by portable userspace code (e.g. `sys::thread_exit`'s
terminal spin loop) without needing an `#if defined(__aarch64__)` guard
at the call site.

## Responsibilities

- Provide `sys::arch::cpu::relax()` and `trigger_illegal_instruction()`,
  selected via this directory's entry on the userspace arch include path
  (mirroring the kernel's `src/arch/<arch>/include` polymorphism, but
  scoped to instructions that are safe to execute unprivileged at EL0).
  The latter backs `src/user/tests/pager_client/main.cc`'s
  illegal-instruction fault-delivery test.

## Invariants

- Contains only EL0-legal instructions. Never reuse the kernel-side
  `src/arch/arm64/include/sys/arch/cpu.hh` from userspace: its
  `current_id()` reads `MPIDR_EL1`, an EL1-only register that SIGILLs at
  EL0.
- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.

## Verification

Exercised indirectly via any userspace path that calls `thread_exit()`
(see `sys/thread.hh`).
