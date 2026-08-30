# Zilch L4 microkernel

A restricted-C++20, header-oriented L4 microkernel skeleton for ARM64/QEMU virt and AMD64/QEMU q35.

The authoritative production requirement tracker is [`docs/readiness/PRODUCTION_READINESS_CHECKLIST.md`](docs/readiness/PRODUCTION_READINESS_CHECKLIST.md). [`docs/roadmap/PROGRAM_CHECKLIST.md`](docs/roadmap/PROGRAM_CHECKLIST.md) is the concise execution index, and [`docs/architecture/alignment/ARCHITECTURE_ALIGNMENT_REVIEW.md`](docs/architecture/alignment/ARCHITECTURE_ALIGNMENT_REVIEW.md) records the alignment review. Historical batch-status documents describe individual increments and are not the current source of truth.

## Repository convention

- `include/abi/`: stable C-compatible kernel/userspace ABI (`.h`)
- `include/sys/`: shared restricted-C++ contracts (`.hh`)
- `src/`: product implementation, colocated module docs and tests
- `samples/guests/`: independently built guest examples and their toolchains
- `tools/`: build, run, release, image and future collection tooling
- `out/`: all generated artifacts

A module is represented by `module.hh`, `module.md`, and `module.tt`. Linkage anchors use `module.cc`; architecture startup uses `.S`.

## Build

The current build accepts `BUILD_VARIANT=development|certification|release`.
The planned Kconfig migration replaces those presets with strict debug and
release defconfigs; see
[`docs/architecture/BUILD_CONFIGURATION.md`](docs/architecture/BUILD_CONFIGURATION.md).

```sh
make arm64
make amd64
make BUILD_VARIANT=certification
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

`printk`, `pr_info`, `pr_warn`, `pr_err`, and `pr_debug` retain a printf-like
call interface. The freestanding formatter consumes a C `va_list`. Supported
conversions are `%c`, `%s`, `%d`, `%i`, `%u`, `%x`, `%X`, `%p`, and `%%`, with
optional `l`, `ll`, or `z` length markers.

The planned `CONFIG_PRINTK_TIME` option prefixes formatted kernel records with
Linux-style boot-relative timestamps such as `[    0.012345]`. Raw guest UART
and emergency output remain separate from formatted kernel logging.

## Hypervisor object-table reservation

Hypervisor Profile 0.1 reserves object-table slots 80 and 81 for the bootstrap VM and vCPU. The lower ranges are already occupied by threads, tasks, endpoints, frames, page tables, notifications, interrupts, scheduling contexts, and address spaces.

The bounded ARM64 ELF64 bootstrap-loader increment and its limitations are
recorded in `docs/history/batches/PRODUCTION_ELF64_LOADER_BATCH.md`.

## Recent architecture-alignment work

- [Product/test separation batch 0079](docs/history/batches/PRODUCT_TEST_SEPARATION_BATCH.md)

## Build ownership

The build is a single non-recursive dependency graph assembled from ownership fragments:

- `src/kernel/kernel.mk` — kernel and architecture mechanism objects;
- `src/user/user.mk` — PL3 programs, libsys/runtime, and the test-guest artifact boundary;
- `src/image/image.mk` — earlyfs and bootstrap packaging;
- `tests/tests.mk` — certification-only adapters;
- `mk/` — shared configuration, toolchain, and verification targets.

Use `make boundary-check` and `make abi-check` to verify private-header isolation and ABI layout/self-containment.

Guest operating-system examples are independently owned under
`samples/guests/`. The first planned relocation is the Zephyr build from
`src/user/guests/zephyr/` to `samples/guests/zephyr/`, with a nested flake and
all fetched/generated content under an ignored sample-local `out/` directory.

## Build layout

Zilch uses one non-recursive build graph. The repository root `Makefile` is the only Make entry point; domain orchestration is defined by `src/kernel/kernel.mk`, `src/user/user.mk`, `src/image/image.mk`, and `tests/tests.mk`. Directory-local object lists use `build.mk` consistently.
