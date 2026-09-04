# Kernel/arch/platform decoupling

Status: **pilot landed, unboot-tested**. Mechanism 1 (init-stage linker
sections) and mechanism 2 (steady-state ops-vtable contracts) are
implemented and building on both arm64 and amd64 for a driver section
holding `timer` (both platforms, real) and `smmu` (arm64: real discovery
only, DEV-006; amd64: honest stub, no IOMMU). The platform registry
(DTB-matched multi-platform selection) remains a design only -- no code
for it exists yet. See "Pilot status" and "Driver section: UART, SMMU,
timer" below for exactly what landed, what was verified, and what
remains -- most importantly, **no QEMU boot test has been possible in
this environment**, so nothing here should be treated as confirmed
working beyond static build/layout checks until `make smoke` actually
runs.

## Problem statement

`src/kernel/`, `src/arch/$(ARCH)/`, and `src/platform/$(PLATFORM_DIR)/` are
already separated at the build level: each compiles to its own `built-in.o`
and the final link combines exactly one arch and one platform chosen by
`ARCH`/`PLATFORM_DIR` (`src/kernel/kernel.mk`). That is compile-time
selection, not a decoupled interface. Concretely:

- `src/kernel/include/sys/kernel/kernel.hh` `#include`s `<sys/arch/arch.hh>`
  and `<sys/platform/platform.hh>`, which resolve through
  `-I src/arch/$(ARCH)/include` / `-I src/platform/$(PLATFORM_DIR)/include`
  (`kernel.mk:6-7`). The kernel's own header search path reaches into a
  specific implementation's private headers.
- `kernel::start()` / `start_secondary()` are long, hand-ordered sequences of
  calls to specifically named functions (`arch::cpu::initialize_boot_cpu()`,
  `arch::hypervisor::initialize_cpu()`, `platform::interrupt::initialize()`,
  `platform::timer::initialize()`, …), duplicated almost identically between
  the two entry points. Every new init step, or new platform, means editing
  `kernel.hh` itself.
- Platform-specific detail leaks into kernel code, e.g. the literal string
  `"gic: initializing distributor and boot CPU interface"` is logged from
  `kernel.hh`, not from the platform that owns the GIC.

There is already a good seed for the right shape:
`include/sys/arch/v1/contract.hh` and `include/sys/platform/v1/contract.hh`
define a neutral, versioned namespace that `kernel.hh::verify_contracts()`
checks with `static_assert`. That compile-time contract-checking should be
kept and extended, not replaced.

## Goals

- Kernel code never `#include`s a per-arch or per-platform private header
  (`src/arch/$(ARCH)/include/...`, `src/platform/$(PLATFORM_DIR)/include/...`).
  It only sees neutral contract headers under `include/sys/{arch,platform}/v1/`.
- Adding an init step, or a new platform backend, does not require editing
  `kernel.hh`.
- Compile-time contract verification (`static_assert` on version/geometry)
  is preserved and extended, not weakened.

## Non-goals

- Runtime-loadable kernel modules. Nothing here is dynamically loaded after
  boot; every backend is statically linked into the image ahead of time.
- Multi-arch images. Exactly one ISA is compiled into a given image; this
  is a compile-time choice and stays that way (see "Arch stays singleton"
  below).
- Generalized (Linux-scale) hardware enablement. This project supports a
  small, explicitly authored set of boards, not arbitrary vendor hardware.

## Two mechanisms, not one

Conflating "one-shot boot sequencing" with "steady-state repeated calls"
is where this kind of redesign usually goes wrong. They need different
mechanisms.

### Mechanism 1 — init-stage linker sections

For one-shot (or once-per-CPU) bring-up steps where the kernel controls
*ordering* but not *mechanism*.

A kernel-owned neutral header, e.g. `include/sys/kernel/init/init.hh`:

```cpp
namespace sys::kernel::init {
    using init_fn_t = error_t (*)(const boot_context_t*) noexcept;
    struct entry_t { init_fn_t fn; const char* name; };
}

#define SYS_INIT(stage, fn) \
    static const ::sys::kernel::init::entry_t __sys_init_##fn \
        __attribute__((used, section(".sys_init_" #stage))) = { fn, #fn }

#define ARCH_INIT(stage, fn)  SYS_INIT(stage, fn)
#define PLAT_INIT(stage, fn)  SYS_INIT(stage, fn)
```

Notes on the signature: every entry shares one signature
(`error_t(*)(const boot_context_t*) noexcept`), even for steps that "can't
fail" — this keeps iteration homogeneous and freestanding (no exceptions,
no per-stage bespoke argument list). `boot_context_t` (firmware/DTB pointer,
cpu id, is-boot-cpu flag) is passed uniformly so a registrant doesn't need
to re-derive context that the kernel already has.

A single source-of-truth stage list (X-macro,
`include/sys/kernel/init/stages.def`), shared by the C++ header and the
linker script:

```
SYS_INIT_STAGE(cpu_early,   percpu)
SYS_INIT_STAGE(hypervisor,  percpu)
SYS_INIT_STAGE(memory_cpu,  percpu)
SYS_INIT_STAGE(hardening,   percpu)
SYS_INIT_STAGE(scheduler_cpu, percpu)
SYS_INIT_STAGE(interrupt,   percpu)
SYS_INIT_STAGE(timer,       percpu)
SYS_INIT_STAGE(memory,      once)
SYS_INIT_STAGE(scheduler,   once)
SYS_INIT_STAGE(smp,         once)
```

The `once` / `percpu` tag lets `kernel::start()` call `run_once_stages()`
and `run_percpu_stages()` in the right places, and lets `start_secondary()`
call only `run_percpu_stages()` — collapsing the current duplication
between the two entry points.

Linker script sections use underscores, not dots (`.sys_init_console`, not
`.sys_init.console`) — GNU ld only auto-generates `__start_SECNAME` /
`__stop_SECNAME` boundary symbols for section names that are valid C
identifiers. Each stage gets `KEEP()`'d start/end markers in `kernel.ld`,
following the existing pattern used for `.bss`/`.stack`:

```
.sys_init : ALIGN(8) {
    __sys_init_cpu_early_start = .;  KEEP(*(.sys_init_cpu_early))  __sys_init_cpu_early_end = .;
    __sys_init_hypervisor_start = .; KEEP(*(.sys_init_hypervisor)) __sys_init_hypervisor_end = .;
    ...
}
```

To avoid the stage list drifting between the C++ header and the `.ld`,
convert `kernel.ld` to `kernel.ld.S` and preprocess it (`-x
assembler-with-cpp`), so `stages.def` generates both sides from one file
(the same trick Linux uses for `vmlinux.lds.S`).

Each entry's `name` field is logged generically
(`pr_info("init: stage=%s fn=%s result=...")`) — this also fixes the
platform-string-in-kernel-code problem: the arch/platform code logs its own
detail from inside its own registered function, not from `kernel.hh`.

### Mechanism 2 — ops-vtable contracts

For anything called repeatedly after boot (`arch::cpu::halt()`,
`platform::timer::ticks()`, `arch::smp::online_count()`, …). Iterating a
section on every call is the wrong tool here, and adds indirection to hot
paths (scheduling, IPC) for no benefit.

Extend the existing `include/sys/arch/v1/` / `include/sys/platform/v1/`
contract headers (today just `types.hh`/`version.hh`/`contract.hh`) with
ops-struct *types*:

```cpp
// include/sys/arch/v1/cpu_ops.hh (kernel-owned, neutral)
namespace sys::arch::v1 {
    struct cpu_ops_t {
        void (*halt)() noexcept;
        void (*relax)() noexcept;
        u32  (*current_id)() noexcept;
        void (*wait_for_event)() noexcept;
    };
}
```

Because exactly one arch is linked into any given image, this does not need
section iteration — a single well-known extern symbol of the contract
type, defined by exactly one TU, is simpler and gives the same decoupling:

```cpp
// src/arch/amd64/arch.cc
extern "C" const sys::arch::v1::cpu_ops_t sys_arch_cpu_ops = { &halt_impl, ... };
```

Bundle all of an arch's or platform's ops sub-structs into one aggregate
root (`arch_ops_t`, `platform_ops_t`) so there is one selected pointer, not
several scattered globals.

**Sections are for mechanism 1 (cardinality can be >1: multiple init
registrants, multiple platform candidates). Singleton extern symbols are
for mechanism 2 (cardinality is always exactly 1 per image).** Using
sections for mechanism 2 would be needless indirection; using a plain
symbol for mechanism 1 doesn't work once there's genuine multiplicity
(see the platform registry below).

### Boundary enforcement

Once both exist, `KERNEL_PRIVATE_INCLUDES` in `kernel.mk` should drop
`-I src/arch/$(ARCH)/include` / `-I src/platform/$(PLATFORM_DIR)/include`
for kernel/ sources entirely. Kernel/ only ever sees
`include/sys/{arch,platform}/v1/`. Those per-target include dirs stay
visible only to the arch/platform/boot `built-in.o`s that implement
against the contract.

## Multiple platforms selected via DTB (arm64 only)

### Why this needs a third mechanism

"Multiple platforms, selected via DTB" is cardinality **N at link time, 1
at runtime**: several board backends compiled into the same image, one
chosen after reading the DTB the bootloader hands you. Neither of the two
mechanisms above expresses that directly — mechanism 2's singleton symbol
can't hold N candidates, and mechanism 1's section iteration runs every
registrant, which is wrong here (you must not touch another board's MMIO
map).

**Arch stays a build-time singleton.** ISA is fixed at compile time; you
never choose amd64 vs arm64 after boot, and different ISAs need entirely
different boot entry code. Only **platform** (board within an arch)
gets runtime multiplicity.

### Registry design

Kernel-owned descriptor type (`include/sys/platform/v1/descriptor.hh`):

```cpp
namespace sys::platform::v1 {
    struct descriptor_t {
        u32 abi_magic;                             // catches stale-build ABI mismatches
        u16 abi_version;
        const char* name;                          // e.g. "qemu_arm64_virt"
        const char* const* compatible;              // NUL-terminated DT root "compatible" strings
        usize_t compatible_count;
        const platform_ops_t* ops;                   // steady-state ops
        const init::entry_t* init_stage[init::stage_count]; // ordered per-stage init hooks
    };
}
```

Registration, in a genuinely multi-entry section:

```cpp
#define PLAT_REGISTER(desc) \
    static const ::sys::platform::v1::descriptor_t* const __plat_reg_##desc \
        __attribute__((used, section(".plat_registry"))) = &desc
```

Each board backend puts one of these in its own `platform.cc`; all
candidate backends compile into the same arm64 image.

Selection runs once, very early, before any `PLAT_INIT` hook:

```cpp
namespace sys::platform {
    const v1::descriptor_t* select(uintptr_t dtb_address) noexcept {
        char root_compat[256];
        if (fdt::root_compatible(dtb_address, root_compat, sizeof(root_compat)) != error_t::success)
            return nullptr; // falls back to the arch-level semihosting panic path
        for (const char* id = root_compat; *id != '\0'; id += strlen(id) + 1) {
            for (auto* const* it = __plat_registry_start; it != __plat_registry_end; ++it)
                for (usize_t i = 0; i < (*it)->compatible_count; ++i)
                    if (fdt::equal(id, (*it)->compatible[i]))
                        return *it;
        }
        return nullptr;
    }
}
```

Match order matters: the DTB's own `compatible` stringlist is ordered
most-specific-first per devicetree convention, so the outer loop walks the
**DTB's** list and the inner loop scans the registry — not the reverse.
Two descriptors claiming the same compatible string is a build/boot-time
error, not "first one wins silently." A match failure is fatal (panic via
the semihosting path below), never a silent fall-through with the wrong
board's MMIO addresses.

Only the *matched* descriptor's `init_stage[]` entries run. Section
iteration is used for discovery/matching (N candidates, one match), not
for blind execution of every registrant.

### Earlycon: resolved by *not* deriving it from the DTB

Initial design considered deriving the console address from the DTB
(`/chosen/stdout-path`, or a nested `.uart_registry` compatible-match at
device granularity). **That approach is rejected.** Reasons:

- It creates exactly the circular fragility earlycon exists to avoid: a bug
  in DTB node-walking for the console loses diagnostic output at the exact
  moment it's needed to debug that bug.
- The DTB is untrusted/bootloader-controlled input; parsing more of it than
  necessary in the most safety-critical part of boot is a cost with no
  matching benefit here.
- This project targets a small, explicitly authored set of boards, not
  general hardware enablement. The author of each board's `platform.cc`
  already knows that board's UART address at the time they write it —
  there is nothing to discover.

**Resolution:** the only DTB read needed is the single root `/compatible`
property already required for board selection (bounded, shallow, simple —
not general node/`reg`-cell walking). Once `.plat_registry` matching picks
a descriptor, its console ops and address are compile-time constants baked
into that board's own `platform.cc`, exactly as `qemu_arm64_virt/console.hh`
does today (`uart_base = 0x09000000ULL`) — just selected from N compiled-in
options instead of being the only option. No `/chosen/stdout-path`
resolution, no per-device `.uart_registry` matching layer.

**Pre-match and match-failure diagnostics** still need a channel, since by
construction no board-specific UART is known yet at that point. Use an
arch-level, DTB-independent fallback: semihosting (`HLT #0xF000` on
QEMU/arm64). This needs no MMIO address and no device tree — it is the one
channel that must not depend on anything this feature is trying to
determine. To confirm before relying on it: verify empirically (a
`tools/verification` smoke run) that the instruction reaches QEMU's
semihosting intercept without requiring the guest's vector table to be
installed yet, rather than assuming it.

### amd64/q35 is out of scope for this mechanism

Checked directly against `src/arch/amd64/boot/start.S` and
`src/platform/qemu_amd64_q35/include/sys/platform/*`:

- Console is legacy ISA port I/O (`outb`/`inb` on `0x03f8`, COM1), not MMIO.
  Port addresses are a PC-platform convention, not board/SoC-specific like
  ARM MMIO — there is nothing to discover, a fixed constant is correct
  across every QEMU x86 machine type, not just one.
- Paging is fully set up in assembly (identity-mapped low 1 GiB, long mode)
  before `sys_kernel_entry` is ever called. There is no pre-MMU window in
  C++ to reason about, unlike arm64.
- There is no DTB on x86; the equivalent would be ACPI (RSDP → MADT/FADT),
  a different format discovered through a different path (Multiboot info
  pointer or UEFI system table), not something the DTB-matching primitive
  generalizes to.
- Incidentally, `start.S` does not currently thread the Multiboot info
  pointer (EBX) into `%rdi` before `call sys_kernel_entry`, matching
  `firmware.hh`'s `boot_info.firmware_data = 0U` constant — amd64 has no
  wired-up firmware-data path today, DTB-style or otherwise. Noted for
  context; out of scope for this design.

The registry+match *pattern* (descriptor list, section-based registration,
runtime match against a discovered id) is architecture-agnostic and would
apply to ACPI-based matching if amd64 ever needs multi-board support. The
*implementation* is not shared — different key space, different discovery
mechanism, different point in boot. Treat as a separate future design if
it comes up.

## Issues catalogued during design, and their resolutions

### Linker/toolchain mechanics

| Issue | Resolution |
|---|---|
| `__start_SECNAME`/`__stop_SECNAME` auto-symbols require valid-C-identifier section names; dotted names (`.sys_init.console`) don't get them. | Use underscore-separated section names, and/or define the boundary symbols explicitly in the linker script rather than relying on the auto-generated form. |
| `--gc-sections` silently drops unreferenced entries. | `KEEP()` every new section, no exceptions (matches the existing `.bss`/`.stack` pattern). Extend `check_elf.sh` to assert each expected section is present and sized as a multiple of `sizeof(entry_t)`. |
| Stage list drifting between the C++ header and the `.ld` file. | Single X-macro (`stages.def`); preprocess `kernel.ld.S` so both sides generate from one source. |
| Function-pointer tables must be read-only (`.rodata`), or hardening is undermined. | `check_elf.sh` verifies these symbols resolve inside the `.rodata` output section; fail the build otherwise. |

### Ordering & concurrency

| Issue | Resolution |
|---|---|
| Multiple registrants in one stage rely on link order, which is implicit and fragile. | Stages declared "singleton" in `stages.def` get a build-time assertion that `(end - start) == sizeof(entry_t)`; only stages explicitly declared "multi" may have more than one entry. |
| "Once" (global) stages must complete before any "percpu" stage runs on any core, including a secondary core that races ahead. | Make the release point explicit: `run_once_stages()` must fully complete with a release fence before `arch::smp::boot_secondary_cpus()` runs — document this as an invariant of the stage-runner, not an accident of call order (which is how it holds today). |
| Registry match order: registry-outer/DTB-inner gets DT specificity priority backwards. | Loop the DTB's own `compatible` stringlist outer (most-specific-first, per DT convention), registry inner; stop at first hit. Duplicate compatible claims across descriptors are a hard build/boot error. |

### Type safety / ABI

| Issue | Resolution |
|---|---|
| One signature for all init entries loses per-stage context. | Pass a single `const boot_context_t*` uniformly to every init/select call instead of having each registrant re-derive cpu id, firmware pointer, etc. |
| Scattered ops globals instead of one handle. | One aggregate `platform_ops_t`/`arch_ops_t` bundling sub-structs; one selected pointer. |
| Silent ABI mismatch if an arch/platform object is stale relative to a changed contract header. | Reuse the pattern already in this codebase (`bootinfo::magic`/`version`): a magic+version field at the front of `descriptor_t` and every ops struct, checked and rejected loudly rather than silently misinterpreted. |

### Security / hardening

| Issue | Resolution |
|---|---|
| Function-pointer tables are a classic control-flow-hijack target, and this design adds several where direct static calls exist today. | All tables `const`/`.rodata`, never mutated post-link (selection only reads, never registers at runtime). Evaluate CFI (`-fsanitize=cfi-icall`, arm64 BTI / x86 CET) for these specific indirect call sites. A CI check can enumerate valid targets per section via `nm`/`objdump` and diff against an expected symbol list. |
| Verification/certification proof technique changes from "static call graph" to "closed-world enumerable indirect set." | Needs a short note in `docs/certification` describing the new proof obligation before this lands, not after. |

### Boot sequencing

| Issue | Resolution |
|---|---|
| No console before platform match. | Resolved by not deriving console from the DTB at all — see "Earlycon" above; matched descriptor's static console is available immediately after the one bounded root-compatible read. |
| A fault during the (now minimal) root-compatible read has no platform ops to report through. | Arch-level semihosting fallback, independent of platform ops, used only for this window. |
| MMU/addressing state when reading the DTB. | Verified non-issue on arm64: console output and DTB access both already happen before `arch::memory::initialize_cpu()` enables the MMU (`sys/arch/memory.hh:194-206`); everything is flat physical-address access at this point, consistent with today's `console::putc()`. |

### Build system / portability

| Issue | Resolution |
|---|---|
| `src/arch/host` likely doesn't use the custom `kernel.ld` (built as a normal hosted ELF for `tools/verification/run_host_kernel_logic.sh`); custom section names have no guaranteed placement under a default linker script. | Either ship a linker-script fragment for the host build too, or give the macros an `ARCH_HOST`-gated alternate definition using `__attribute__((constructor))` to push into a runtime array — host has a real C runtime, this is fine there. |
| Combinatorial matrix: ARCH × single-platform vs. platform-bundle × which boards are bundled. | Keep single-descriptor build as the default/certified profile (today's behavior, no runtime matching). Make the DTB-bundle profile an explicit opt-in Kconfig choice with its own CI job that boots the same bundled image under QEMU with each supported `-machine` and asserts the correct descriptor matched. |

### Migration

| Issue | Resolution |
|---|---|
| Flag-day rewrite risk across `kernel.hh` and every arch/platform `.cc`. | Land the mechanism (macros, sections, stage runner) inert first. Migrate one subsystem end-to-end as a pilot — `timer` is the recommended first candidate: smallest ops surface, single init call, exercised in both `start()` and `start_secondary()`. Validate boot and existing selftests (`CONFIG_SELFTEST`) still pass before migrating the rest stage-by-stage. |

## Open questions

- Confirm semihosting reaches QEMU's intercept without vector-table setup
  (empirical check against `tools/verification`, not assumed).
- Decide the exact `boot_context_t` field set before writing `init.hh`.
- Decide whether a platform-match failure should halt or degrade to a
  reduced/root-only boot mode — currently proposed as fatal.

## Recommended next step

Prototype mechanism 1 + mechanism 2 against a single subsystem (`timer`)
on `qemu_arm64_virt` only, without introducing the platform registry yet,
to validate the linker mechanics (section placement, `KEEP()`, boundary
symbols, `.rodata` placement) before extending to multi-platform selection.

## Pilot status

The step above has landed, for both `qemu_arm64_virt` and `qemu_amd64_q35`
(not arm64-only, since `kernel.hh` is shared between both and both needed
matching linker-script/`platform.cc` changes to keep building).

### What changed

- `src/kernel/include/sys/kernel/init/init.hh` (new): `boot_context_t`,
  `entry_t`, `run_stage()`, and the `SYS_INIT`/`ARCH_INIT`/`PLAT_INIT`
  macros described under "Mechanism 1" above. Only the `timer` stage
  exists; the full `stages.def` X-macro list is not built yet.
- `include/sys/platform/v1/timer_ops.hh` (new): `timer_ops_t` contract
  type and the `extern "C" sys_platform_timer_ops` declaration, per
  "Mechanism 2".
- `src/arch/arm64/kernel.ld` and `src/arch/amd64/kernel.ld`: added the
  `.sys_init_timer` section (`KEEP()`'d, with `__sys_init_timer_start`/
  `_end` boundary symbols) inside the existing `.rodata` output section.
- `src/platform/qemu_arm64_virt/platform.cc` and
  `src/platform/qemu_amd64_q35/platform.cc`: each now defines a
  `timer_percpu_init` wrapper registered via `PLAT_INIT(timer, ...)`, and
  populates `sys_platform_timer_ops` pointing at the existing
  `sys::platform::timer::*` free functions — no timer logic changed on
  either platform, only exposed through the new contract type in addition
  to the existing one.
- `src/kernel/include/sys/kernel/kernel.hh`: the two
  `platform::timer::initialize()` call sites (`start()`,
  `start_secondary()`) now go through `init::run_stage()`; the boot-path
  reads of `ticks_per_second`, `ticks()`, and `certification_valid()` now
  go through `sys_platform_timer_ops` instead of the direct
  `platform::timer::` calls.

### What was deliberately left out of scope

- **Every other `platform::timer::` call site** — `scheduler.hh`,
  `ipc.hh`, `control.hh`, `interrupt.hh`, and both `arch.cc` files still
  call the free functions directly. These are IPC/scheduler hot paths;
  converting them to indirect ops-pointer calls is a separate, more
  carefully reviewed change, not part of validating the linker mechanics.
  Practically, this means `kernel.hh` still `#include`s
  `<sys/platform/platform.hh>` and is not yet free of per-platform header
  dependencies — that end-state goal needs the remaining subsystems
  (console, interrupt, memory, firmware) migrated too, plus these
  remaining timer call sites.
- **The platform registry** (`.plat_registry`, DTB root-`compatible`
  matching, `platform::select()`) — design only, no code.
- **The full `stages.def` stage list** (`cpu_early`, `hypervisor`,
  `memory`, `hardening`, `scheduler`, `interrupt`, `smp`, …) — only
  `timer` exists as a stage; `kernel.hh`'s other boot-sequence calls are
  unchanged.
- **`check_elf.sh` assertions** for section presence/size/`.rodata`
  placement — verified manually for this pilot (see below), not yet
  encoded as an automated build gate.
- **`src/arch/host`** — not touched; the design doc's concern about the
  host build's default linker script not knowing about `.sys_init_timer`
  is real but unexercised here, since nothing under `tests/host/` includes
  `kernel.hh` today (confirmed by grep before treating this as out of
  scope).

### What was verified, and how

- `make arm64` and `make amd64` both build clean (`BUILD_VARIANT=debug`,
  the default), including `make format-check` reporting no violations in
  any file touched by this pilot (pre-existing violations in untouched
  files remain, unrelated to this change).
- `nm`/`readelf` against both resulting `zilch.elf` binaries confirm:
  - Exactly one `.sys_init_timer` entry on each (`__sys_init_timer_end -
    __sys_init_timer_start == 16` bytes == one `entry_t`), i.e. the
    "singleton stage" assumption from the design actually holds today and
    the entry survived `--gc-sections` because of `KEEP()`.
  - Both `__sys_init_timer_start`/`_end` and `sys_platform_timer_ops` fall
    inside the `.rodata` output section on both arm64 and amd64 — the
    read-only-placement requirement from the "Security / hardening" table
    above holds, checked directly against section addresses rather than
    assumed.

### What was **not** verified

- **No boot test.** This environment has no `qemu-system-aarch64` or
  `qemu-system-x86_64` available (package install failed on missing
  mirror files), so `make run` / `make smoke` were not exercised. The
  build and static-layout checks above give real confidence in the
  linker mechanics specifically, but not that the kernel still boots to
  the same state it did before this change. Run `make smoke` (or `make
  run`) on a machine with QEMU before trusting this beyond the linker
  mechanics.
- `tools/verification/run_host_kernel_logic.sh` was attempted but fails in
  this environment for an unrelated, pre-existing reason (missing
  `libclang_rt.asan*` static libraries for the installed clang); confirmed
  it doesn't touch `kernel.hh`/`init.hh` either way, so it wasn't
  informative for this change even where it does run.

## Driver section: UART, SMMU, timer

A "driver" in this codebase is not a new mechanism — it's mechanism 1
(init-stage registration) plus mechanism 2 (ops-vtable contract) applied
to a platform subsystem, with a name. This section covers extending the
timer pilot to a second, structurally different slot: SMMU/IOMMU.

### Why SMMU is not the same shape as timer or console

UART and timer have real, working implementations to point the pattern
at. SMMU/IOMMU did not, at the time this was proposed: zero code existed,
and `docs/readiness/KERNEL_SECURITY_MODEL.md` explicitly states DMA
isolation before this subsystem exists is outside the certified boundary,
with 18 unchecked items (`DEV-001`..`DEV-018`) in
`docs/readiness/PRODUCTION_READINESS_CHECKLIST.md` §9. Full SMMUv3 support
(stream tables, per-domain translation contexts, command/event queues,
invalidation, DMA-to-capability binding) is a multi-week undertaking this
pass does not attempt. What it does implement, for real:

**`DEV-006` ("ARM SMMU discovery implemented"), and only that.**

### What changed

- `include/sys/platform/v1/smmu_ops.hh` (new): `smmu_ops_t` contract —
  `present()`, `idr0()`, `idr1()`, `stage1_supported()`,
  `stage2_supported()`, `stream_id_bits()`. All-scalar returns, matching
  `timer_ops_t`'s style rather than returning an aggregate struct by value
  across the `extern "C"` boundary.
- `src/platform/qemu_arm64_virt/include/sys/platform/smmu.hh` (new): the
  real driver. Read-only MMIO probe of QEMU virt's fixed `VIRT_SMMU`
  address (`0x09050000`, size `0x20000`, from QEMU's `hw/arm/virt.c`
  `base_memmap`, confirmed against QEMU's own source rather than assumed)
  and IDR0/IDR1/IIDR/AIDR register decode (offsets and bitfields from ARM
  IHI 0070, cross-checked against Linux's
  `drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h` rather than relied on from
  memory alone). `CR0` (translation enable) is never written. Presence
  detection relies on QEMU's virt platform bus returning `0xffffffff` for
  unassigned MMIO reads — a QEMU-specific heuristic, documented as such in
  the file, acceptable because this driver only ever targets that one
  board.
- `src/platform/qemu_arm64_virt/platform.cc`: registers `smmu_init` via
  `PLAT_INIT(smmu, ...)` (a **once** stage — SMMU is one shared platform
  device, not per-CPU state, unlike timer's **percpu** stage) and
  populates `sys_platform_smmu_ops` pointing at the real driver.
- `src/platform/qemu_amd64_q35/platform.cc`: registers a stub `smmu_init`
  that logs "not present" and populates `sys_platform_smmu_ops` with
  functions that always report absence. This is not a placeholder for
  missing work — Intel VT-d is a distinct spec/register model from ARM
  SMMUv3 and is genuinely out of scope; reporting "no IOMMU on this
  platform" is a complete, honest implementation of the contract for a
  platform that doesn't have one.
- `src/arch/arm64/kernel.ld` / `src/arch/amd64/kernel.ld`: originally
  added a fourth and fifth per-driver section (`.sys_init_smmu`,
  `.sys_ops_timer`, `.sys_ops_smmu`) mirroring `.sys_init_timer` — **since
  superseded** by the generic, driver-agnostic design in "Revision: one
  generic section per mechanism, not one per driver" below. The linker
  scripts now carry exactly two permanent rules total, not one pair per
  driver.
- `src/kernel/include/sys/kernel/kernel.hh`: runs the `smmu` stage once,
  from `start()` only (never `start_secondary()`), right after GIC
  initialization and before the timer stage.
- `tools/run/run.sh`: added `QEMU_SMMU=1` (default off) to append
  `iommu=smmuv3` to both `-machine` invocations. Default behavior is
  unchanged — this was **not** turned on unconditionally, because this
  session had no QEMU available to verify that enabling QEMU's SMMUv3
  model doesn't affect other emulated devices' DMA paths (virtio-blk,
  virtio-net). That verification is still outstanding.

### A real finding from this pass: `used` alone doesn't survive `--gc-sections`

Building `sys_platform_smmu_ops` first without a dedicated section (just
`extern "C" __attribute__((used)) const ...`) — reasoning that `used`
should be enough since nothing else in this codebase needed more — the
symbol **vanished from the linked binary's symbol table entirely** on
both arches. `sys_platform_timer_ops` survived only because `kernel.hh`
actually reads its fields at boot; `sys_platform_smmu_ops` has no reader
yet, so nothing kept its auto-generated `-fdata-sections` subsection alive
under `--gc-sections`. `__attribute__((used))` only tells the *compiler*
not to treat something as dead code — it does not tell the *linker* to
retain an unreferenced section. The fix is the same mechanism already
proven for `.sys_init_timer`: a named section (`.sys_ops_timer`/
`.sys_ops_smmu`) plus `KEEP()` in the linker script — with the added
wrinkle that clang requires the `section()` attribute to match on
*every* declaration of the symbol, not just the defining one, so the
`extern` declarations in `smmu_ops.hh`/`timer_ops.hh` needed the
attribute too (`-Werror -Wsection` otherwise). This is a genuinely useful
correction to the "Linker/toolchain mechanics" table above: **any ops
singleton without a guaranteed reader needs the named-section+`KEEP()`
treatment, not just multi-registrant init-stage sections.**

### Revision: one generic section per mechanism, not one per driver

The design above still needed a linker-script edit for every new driver
(a fresh `.sys_init_<stage>` and, if it had steady-state ops, a fresh
`.sys_ops_<name>`) — exactly the maintenance burden the whole point of
this exercise was to avoid. Reworked so that **adding or removing a
driver never touches `kernel.ld` again**:

- `sys::kernel::init::entry_t` gained a `stage_t stage` field. Every
  `SYS_INIT`/`ARCH_INIT`/`PLAT_INIT` registration, regardless of which
  driver or stage, now lands in the same fixed, literally-named section:
  `.sys_driver`. `run_stage(stage_t, ctx)` iterates the *entire* combined
  table and calls only the entries whose `stage` field matches — one
  shared table instead of a per-stage linker sub-range.
- Every ops-vtable singleton (`sys_platform_timer_ops`,
  `sys_platform_smmu_ops`, future ones) similarly shares one fixed section,
  `.sys_ops`, via a `SYS_OPS`/`SYS_OPS_DECL` macro pair in `init.hh` (split
  because clang rejects `__attribute__((used))` on a non-defining
  declaration — the neutral contract header's `extern` uses
  `SYS_OPS_DECL` (`section()` only), the defining instance in each
  `platform.cc` uses `SYS_OPS` (`section()` + `used`)).
- Both `kernel.ld` files now carry exactly **two** permanent rules,
  written once:
  ```
  __sys_drivers_start = .;
  KEEP(*(.sys_driver))
  __sys_drivers_end = .;
  KEEP(*(.sys_ops))
  ```
  Adding a new driver, or a new stage for an existing one, is now
  entirely contained to that driver's own `.cc` file (a `PLAT_INIT(...)`
  call and/or a `SYS_OPS`-tagged ops struct) and, only for a genuinely new
  *boot phase* (not a new driver), one enumerator added to `stage_t` in
  `init.hh` — never the linker script.
- `kernel.hh` no longer declares any `extern "C" const entry_t
  __sys_init_<x>_start/_end[]` symbols at all — those were removed
  entirely. Call sites simplified from `init::run_stage(__sys_init_timer_
  start, __sys_init_timer_end, ctx)` to `init::run_stage(init::stage_t::
  timer, ctx)`.

### What was verified, and how

- `make arm64` and `make amd64` both build clean, `make format-check`
  clean on every touched file, after both the original per-driver-section
  version and the generic revision above.
- `nm`/`readelf` confirm, on both arches: the combined `.sys_driver` table
  holds exactly two entries (`__sys_drivers_end - __sys_drivers_start ==
  48` bytes == 2 × `sizeof(entry_t)`, surviving `--gc-sections` via one
  `KEEP()` rule), and both `sys_platform_timer_ops` and
  `sys_platform_smmu_ops` are present and fall entirely inside the
  `.rodata` output section on both targets.
- The QEMU `VIRT_SMMU` base address/size and the `iommu=smmuv3` machine
  property name were confirmed against QEMU's own `hw/arm/virt.c` source
  (via web fetch), not assumed from memory. The IDR0/IDR1 register offsets
  and bitfields were confirmed against Linux's
  `arm-smmu-v3.h` the same way.

### What was **not** verified

- **No boot test, same as the timer pilot** — this environment has no
  QEMU. The discovery code has never actually run against real (emulated)
  SMMUv3 hardware. Static review and cross-referenced register offsets
  are not a substitute for that, especially for a security-relevant
  subsystem — run `QEMU_SMMU=1 make smoke` (or `run`) before trusting the
  discovery output, and plain `make smoke` to confirm `QEMU_SMMU`-unset
  behavior is unaffected.
- **Whether enabling QEMU's SMMUv3 model affects other devices** — not
  established, which is exactly why `QEMU_SMMU` defaults off in `run.sh`
  rather than being folded into the default machine string.
- `docs/readiness/PRODUCTION_READINESS_CHECKLIST.md`'s `DEV-006` checkbox
  was **deliberately left unchecked** — this is a certification tracking
  document, and marking a security-relevant item complete without a real
  boot test would be a false claim, not a conservative one. Check it off
  only after the boot test above actually confirms discovery works
  end-to-end against QEMU's real SMMUv3 model.

## Suggested next slice

Pick one of: (a) migrate the remaining `platform::timer::` call sites in
`scheduler.hh`/`ipc.hh`/`control.hh`/`interrupt.hh`/`arch.cc` through
`sys_platform_timer_ops` (the hot-path conversion the timer pilot
explicitly deferred), (b) migrate `console` next (smallest remaining ops
surface: `initialize()` + `putc()`, two call sites) to make progress on
removing `<sys/platform/platform.hh>` from `kernel.hh` entirely, or (c)
get the SMMU discovery driver boot-tested (`QEMU_SMMU=1 make smoke`) on a
machine with QEMU, which nothing above has been able to do. (c) is the
highest-value next step given SMMU is the one piece of this pass that
touches a documented security boundary and has had zero dynamic
verification so far.
