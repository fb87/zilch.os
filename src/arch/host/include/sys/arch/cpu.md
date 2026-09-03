# Module: cpu (host)

## Purpose

Hardware-free stand-in for `sys::arch::cpu`, used only by
`tools/verification/run_host_kernel_logic.sh` to compile and natively run
`tests/host/kernel_logic.cc` on the machine invoking it. This is not a real
target architecture and is never selected by `ARCH=` for a kernel build.

## Responsibilities

- Satisfy the `sys::arch::cpu` interface `sys::kernel::object::table.hh`
  requires, without touching any actual CPU-identification or
  inter-processor-hint instruction.
- Compile and execute correctly as an ordinary unprivileged process on any
  host architecture, unlike a real arch's `cpu.hh` (see `cpu.hh`'s header
  comment for why matching either amd64's or arm64's own tree fails on some
  hosts, in two different ways).

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- No inline assembly and no architecture-specific instruction of any kind:
  the entire point of this tree is to depend on nothing hardware-specific.

## Verification

Exercised indirectly by `tests/host/kernel_logic.cc` via
`run_host_kernel_logic.sh` (`make host-tests`) -- compiled and linked into
that binary on every host it runs on, though `current_id()`/`relax()`
themselves are never called by that test's three property functions.
