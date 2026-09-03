# Module: timer (host)

## Purpose

Hardware-free stand-in for `sys::arch::timer`, used only by
`tools/verification/run_host_kernel_logic.sh` alongside this tree's
`cpu.hh`. Not a real target architecture.

## Responsibilities

- Satisfy the `sys::arch::timer` interface without reading any real
  hardware counter register.
- Provide a value that increases on each call, sufficient for any code path
  that merely needs successive reads to differ, without claiming to
  represent real elapsed time (`frequency()` returns 0, matching amd64's
  own placeholder for the same reason).

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- No inline assembly and no architecture-specific instruction of any kind.

## Verification

Exercised indirectly by `tests/host/kernel_logic.cc` via
`run_host_kernel_logic.sh` (`make host-tests`).
