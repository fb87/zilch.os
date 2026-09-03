# Module: elf64_dynamic_check (amd64)

## Purpose

`validate_elf64_dynamic_loader()`, called from wherever this architecture's
boot-time or certification self-checks assert that the dynamic ELF loading
path is sound.

## Responsibilities

- Report whether `elf64::load_dynamic()` (frame-per-page backing, the path
  `address_space.hh` actually uses) behaves correctly against a real
  allocator.
- Currently returns `true` unconditionally: amd64 has no dynamic-loader
  self-check exercising real allocation the way arm64's same-named file
  does, since this codebase has not yet needed one on amd64 beyond what
  `address_space.hh`'s own `initialize()` already exercises directly.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Kept as its own file, not folded into `elf64.hh`, purely for structural
  parity with arm64's equivalent file of the same name -- see arm64's
  version for why THAT split exists (a real circular-include hazard there;
  amd64 does not yet have the dependency that would create one, but keeping
  the same file boundary means adding it later does not require moving code).

## Verification

The colocated `elf64_dynamic_check.tt` file is reserved for module-level
tests.
