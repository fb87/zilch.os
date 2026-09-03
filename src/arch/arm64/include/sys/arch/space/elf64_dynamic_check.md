# Module: elf64_dynamic_check (arm64)

## Purpose

De-risking gate for `elf64::load_dynamic()` (frame-per-page backing, the
allocator-driven loading path) before it is ever wired into
`address_space::initialize()` in place of the existing, already-proven
`elf64::load()` (fixed scratch-buffer backing). That cutover is a separate,
later step and is not performed by this file.

## Responsibilities

- `check_dynamic_loader_role(role)`: runs both loaders against the same
  real embedded image and asserts they agree exactly -- same
  status/entry/page-count, identical per-page permissions, and
  byte-identical page contents.
- Kept as its own file rather than folded into `elf64.hh`: this needs
  `sys::kernel::memory` for the physical-page allocator, and
  `sys/kernel/space/address_space.hh` already includes
  `sys/arch/space/address_space.hh` and embeds it by value, so having
  `address_space.hh` itself depend on `kernel/memory/manager.hh` would
  create a circular include. This file sits outside that cycle.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.

## Verification

Called from wherever this architecture's boot-time or certification
self-checks assert the dynamic loader is sound; see call sites of
`check_dynamic_loader_role()`.
