# Production ELF64 loader batch — 0077

## Delivered

- ARM64 ELF64 program-header parser and bounded bootstrap loader.
- Private per-address-space image storage instead of direct mapping of a shared
  raw binary embedded in the kernel.
- Validation of ELF identity, machine, program-header bounds, segment bounds,
  `p_filesz <= p_memsz`, virtual range, alignment, segment overlap, W^X, and
  executable entry point.
- File-data copy and BSS zeroing before mappings become visible.
- Final per-page RX, R, or RW permissions.
- Userspace linker contract fixed to 4 KiB maximum page size.
- Pager-client BSS probe providing runtime integration evidence.

## Explicit limitations

- Bootstrap images remain selected from an embedded registry by role.
- `earlyfs.tar` is built as an image artifact but is not yet discovered and
  parsed by the boot path.
- The loader uses a bounded 64 KiB per-address-space bootstrap buffer rather
  than frames dynamically delegated by the memory server.
- TLS, argv/envp, auxiliary vectors, signatures, and dynamic linking are not
  implemented.

Accordingly this batch advances USR-013 through USR-015 but does not complete
those production-readiness requirements.
