# ARM64 bootstrap ELF64 loader

## Scope

This module replaces direct raw-binary mapping for the ARM64 bootstrap images.
It parses ELF64 program headers, validates AArch64/little-endian identity,
checks all bounds and overflow cases, rejects writable-executable mappings,
requires the entry point to lie in an executable `PT_LOAD`, copies file-backed
bytes into private address-space storage, zero-fills BSS, and installs final
page permissions.

The loader is intentionally bounded to the existing 64 KiB bootstrap image
window. It consumes the independently linked ELF images from the controlled
bootstrap registry. It does not yet locate files by path from earlyfs and does
not replace dynamic frame delegation by the userspace memory server. Therefore
USR-013 remains IN PROGRESS rather than COMPLETE.

## Failure and rollback

Destination storage, permission metadata, and the level-3 table are cleared
before parsing. No mapping is published until validation and copying complete.
A rejected image leaves no executable user mapping and causes thread/process
validation to fail instead of entering PL3.

## Runtime evidence

The certification pager clients exercise two distinct address spaces loaded
from `pager-client.elf`. That image includes a writable zero-sized file segment
with an eight-byte BSS object; each client verifies that the object begins at
zero before continuing. The existing `userspace_pager_service` PASS result now
therefore covers ELF entry selection, RX/RW separation, BSS zero-fill, and
independent image storage.
