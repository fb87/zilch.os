# Batch 0174: bounded Kernel 1.0 core gate

## Completed locally

The certification runner now emits `kernel_core_1_0_gate`, aggregating the
completed capability, IPC, memory, scheduler, interrupt/timer/platform, and
security workloads. Final acceptance independently validates every kernel
database and architectural/platform invariant.

CI has separate certification and release jobs. The certification job boots
four-CPU ARM64 QEMU and requires the zero-failure acceptance record. Release
checks cover ARM64 production source/ELF/instruction/section/stack gates,
AMD64 compile-only ELF/section/stack gates, ABI, boundaries, UBSan,
documentation layout, and reproducibility.

## Production-ready blockers

This batch does not claim the overall Kernel 1.0 production gate. The checklist
still requires:

- a production userspace management-domain service graph;
- retained 24/72-hour and reboot certification;
- the remaining documentation/conformance deliverables;
- retained certification on a real ARM64 hardware platform.

Those outcomes require userspace implementation, elapsed soak time, and
external hardware evidence; QEMU kernel-core success cannot substitute for
them.
