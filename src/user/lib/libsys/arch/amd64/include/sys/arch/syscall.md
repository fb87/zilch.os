# Module: syscall

## Purpose

amd64's raw syscall trampolines: the `syscall` register-marshalling
sequence for each of the five entry points `src/user/lib/libsys/syscall.cc`
exposes as `extern "C"` functions. Isolating the register convention here
lets that file call `sys::arch::syscall::*` unconditionally, with no
`#if defined(__x86_64__)` at the call site.

## Responsibilities

- Provide `invoke_raw`, `ipc_invoke_raw`, `invoke_result1_raw`,
  `ipc_exchange_raw`, and `hypervisor_invoke_raw`, matching the signatures
  `syscall.cc`'s `extern "C"` wrappers delegate to.
- Encode amd64's calling convention: arguments in `rdi`/`rsi`/`rdx`/`r10`/
  `r8`/`r9`, syscall number in `rax`, trap via `syscall` (clobbers `rcx`/
  `r11`).
- Everything past the single raw `invoke_raw` trampoline is built on top of
  it in plain C++: amd64 has no dedicated fast-path IPC syscall entry yet,
  so `ipc_invoke_raw` honestly reports `unsupported` for requests that need
  out-of-line transfer/timeout descriptors instead of silently dropping them,
  and the two-return-register and hypervisor-invoke wrappers derive their
  result from a single `invoke_raw` call.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- `invoke_raw`'s register/clobber list must stay in sync with the kernel's
  amd64 `SYSCALL` entry stub (`src/arch/amd64/include/sys/arch/syscall/entry.hh`)
  — these two files encode opposite ends of the same trap convention.

## Verification

Exercised by every userspace syscall this kernel supports; any control-plane
or IPC test exercises this path.
