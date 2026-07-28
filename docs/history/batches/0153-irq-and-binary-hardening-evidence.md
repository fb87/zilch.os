# Batch 0153: IRQ timing and binary hardening evidence

This batch adds a release-image hardening gate while documenting the separate
IRQ-timing follow-up.

- Audit release ELF section flags for writable/executable overlap, executable
  text, and writable rodata.
- Wire the section audit into both production release architecture paths.

IRQ-disabled timing remains SCH-017 follow-up work; hardware-specific latency
targets and silicon qualification remain separate production work.
