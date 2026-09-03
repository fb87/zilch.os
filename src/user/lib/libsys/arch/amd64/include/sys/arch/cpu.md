# Module: cpu

## Purpose

amd64's userspace-safe CPU helper: a single ring-3-legal spin-wait hint
(`pause`) used by portable userspace code (e.g. `sys::thread_exit`'s
terminal spin loop) without needing an `#if defined(__x86_64__)` guard
at the call site.

## Responsibilities

- Provide `sys::arch::cpu::relax()` and `trigger_illegal_instruction()`,
  selected via this directory's entry on the userspace arch include path
  (mirroring the kernel's `src/arch/<arch>/include` polymorphism, but
  scoped to instructions that are safe to execute unprivileged at ring 3).
  The latter backs `src/user/tests/pager_client/main.cc`'s
  illegal-instruction fault-delivery test.

## Invariants

- Contains only ring-3-legal instructions. Never reuse the kernel-side
  `src/arch/amd64/include/sys/arch/cpu.hh` from userspace: its
  `cpuid`-based helpers are fine unprivileged, but living there mixes
  kernel-only and user-safe arch code in one tree.
- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.

## Verification

Exercised indirectly via any userspace path that calls `thread_exit()`
(see `sys/thread.hh`).
