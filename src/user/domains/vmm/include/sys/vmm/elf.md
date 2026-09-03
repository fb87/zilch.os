# Module: elf (vmm)

## Purpose

ELF64 guest-image layout and header validation, as read by an unprivileged
userspace VMM placing a guest kernel into guest-physical memory. See
`src/user/domains/vmm/README.md` for the split rationale (capabilities, not
subject matter) and why this is deliberately not the same code as the
kernel's own `sys::arch::space::elf64` loaders.

## Responsibilities

- `valid()`: answer whether a header can be trusted (magic, class,
  endianness, `EM_AARCH64`, version) before any offset it contains is used.
- Define `header`/`program_header`/`section_header` matching the on-disk
  ELF64 layout, `__attribute__((packed))` since this is untrusted input
  read at arbitrary alignment, not a structure the compiler is free to lay
  out.

## Invariants

- Pure data and validation logic: no capability, no syscall, nothing that
  requires the authority `src/user/servers/domain` holds.
- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- A guest image failing `valid()` is not necessarily an error -- raw binary
  images are loaded flat instead; this only answers "is this ELF", not
  "is this guest image acceptable".

## Verification

Exercised by `src/user/servers/domain/main.cc`'s `load_guest_image()` for
every guest the domain manager loads, ELF or otherwise.
