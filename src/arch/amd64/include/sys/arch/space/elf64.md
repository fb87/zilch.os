# Module: elf64 (amd64)

## Purpose

ELF64 loader for zilch's own userspace binaries on amd64: parses program
headers, maps segments with the correct permissions, and returns the entry
point. A near-verbatim port of arm64's loader of the same name -- the only
architecture-specific line is `elf_machine_x86_64 = 62U` in place of
`elf_machine_aarch64`; the byte-span-in/page-permission-out logic is
otherwise portable and intentionally kept textually close to arm64's
version so the two stay easy to diff against each other.

## Responsibilities

- Validate the ELF header (magic, class, machine, version) before trusting
  any offset it contains.
- `load()` and `load_dynamic()` (the caller-supplied-allocator variant
  `address_space.hh` actually uses): walk `PT_LOAD` program headers,
  request one physical page per segment page from the caller's allocator,
  and record the permissions (`elf64::page_permissions`) the caller applies
  to its own page-table format.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- This is NOT the same loader as `sys::vmm::elf` (see
  `src/user/domains/vmm/include/sys/vmm/elf.hh`): that one is unprivileged
  userspace code parsing an untrusted guest kernel image; this one runs in
  kernel context loading zilch's own trusted userspace binaries. They share
  only the on-disk ELF64 layout, deliberately not a common implementation.

## Verification

The colocated `elf64_dynamic_check.hh`/`.md` exercises `load_dynamic()`
against a real allocator. Not yet exercised under actual amd64 execution
(see `entry.md`'s note on this platform's QEMU multiboot limitation).
