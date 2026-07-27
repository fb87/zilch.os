# Zilch L4 microkernel

A restricted-C++20, header-oriented L4 microkernel skeleton for ARM64/QEMU virt and AMD64/QEMU q35.

The authoritative production requirement tracker is [`docs/readiness/PRODUCTION_READINESS_CHECKLIST.md`](docs/readiness/PRODUCTION_READINESS_CHECKLIST.md). [`docs/roadmap/PROGRAM_CHECKLIST.md`](docs/roadmap/PROGRAM_CHECKLIST.md) is the concise execution index, and [`docs/architecture/alignment/ARCHITECTURE_ALIGNMENT_REVIEW.md`](docs/architecture/alignment/ARCHITECTURE_ALIGNMENT_REVIEW.md) records the alignment review. Historical batch-status documents describe individual increments and are not the current source of truth.

## Repository convention

- `include/abi/`: stable C-compatible kernel/userspace ABI (`.h`)
- `include/sys/`: shared restricted-C++ contracts (`.hh`)
- `src/`: product implementation, colocated module docs and tests
- `tools/`: build, run, release, image and future collection tooling
- `out/`: all generated artifacts

A module is represented by `module.hh`, `module.md`, and `module.tt`. Linkage anchors use `module.cc`; architecture startup uses `.S`.

## Build

```sh
make arm64
make amd64
make format-check
make release VERSION=0.1.0
```

## File conventions

- `.cc`: C++ translation unit
- `.hh`: C++ interface or inline implementation
- `.S`: preprocessed assembly
- `.md`: colocated module design
- `.tt`: colocated module tests

The tree intentionally contains no project-owned `.h` headers.

## Kernel logging

`printk`, `pr_info`, `pr_warn`, `pr_err`, and `pr_debug` retain a printf-like call interface. The freestanding implementation uses compile-time C++ argument dispatch rather than `va_list`, avoiding early-boot variadic ABI dependencies. Supported conversions are `%c`, `%s`, `%d`, `%i`, `%u`, `%x`, `%X`, `%p`, and `%%`, with optional `l`, `ll`, or `z` length markers.

## Hypervisor object-table reservation

Hypervisor Profile 0.1 reserves object-table slots 80 and 81 for the bootstrap VM and vCPU. The lower ranges are already occupied by threads, tasks, endpoints, frames, page tables, notifications, interrupts, scheduling contexts, and address spaces.

The bounded ARM64 ELF64 bootstrap-loader increment and its limitations are
recorded in `docs/history/batches/PRODUCTION_ELF64_LOADER_BATCH.md`.

## Recent architecture-alignment work

- [Product/test separation batch 0079](docs/history/batches/PRODUCT_TEST_SEPARATION_BATCH.md)

## Build ownership

The build is a single non-recursive dependency graph assembled from ownership fragments:

- `src/kernel/kernel.mk` — kernel and architecture mechanism objects;
- `src/user/user.mk` — PL3 programs, libsys/runtime, and guest executables;
- `src/image/image.mk` — earlyfs and bootstrap packaging;
- `tests/tests.mk` — certification-only adapters;
- `mk/` — shared configuration, toolchain, and verification targets.

Use `make boundary-check` and `make abi-check` to verify private-header isolation and ABI layout/self-containment.
