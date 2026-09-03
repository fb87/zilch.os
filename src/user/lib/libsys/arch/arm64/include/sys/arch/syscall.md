# Module: syscall

## Purpose

arm64's raw syscall trampolines: the `svc #0` register-marshalling
sequences for each of the five entry points `src/user/lib/libsys/syscall.cc`
exposes as `extern "C"` functions. Isolating the register convention here
lets that file call `sys::arch::syscall::*` unconditionally, with no
`#if defined(__aarch64__)` at the call site.

## Responsibilities

- Provide `invoke_raw`, `ipc_invoke_raw`, `invoke_result1_raw`,
  `ipc_exchange_raw`, and `hypervisor_invoke_raw`, matching the signatures
  `syscall.cc`'s `extern "C"` wrappers delegate to.
- Encode arm64's calling convention: arguments in `x0`-`x7`, syscall number
  in `x8`, trap via `svc #0`.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Register clobber lists must stay in sync with the kernel's arm64 syscall
  entry stub (`src/arch/arm64/include/sys/arch/syscall/entry.hh`) — these
  two files encode opposite ends of the same trap convention.

## Verification

Exercised by every userspace syscall this kernel supports; any control-plane
or IPC test exercises this path.
