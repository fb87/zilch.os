# Module: exception (amd64)

## Purpose

`frame_t` (the register-save layout `common_handler` in
`vectors_high.S` writes and every syscall/fault handler reads) and real
IDT construction: `initialize_current_el()` builds and loads a genuine
256-entry IDT pointing at the generated ISR stubs, so this architecture can
actually field an interrupt or exception -- not yet true before this file's
`initialize_idt()` existed (see `vectors_low.md`'s history note).

## Responsibilities

- `frame_t`: general-purpose registers plus vector/error_code/RIP/CS/
  RFLAGS/RSP/SS, in the exact order `common_handler` pushes them --
  verified by this file's own `static_assert`s on `sizeof`/offsets, since
  nothing else at compile time connects the assembly push order to this
  struct's layout.
- `initialize_idt()`: read `sys_amd64_vector_table` (the generated
  address-per-vector table), encode all 256 interrupt-gate descriptors
  (present, DPL0, target selector `gdt::kernel_code_selector`), and `lidt`.
- `initialize_current_el()`: the call site `kernel.hh` already invokes on
  every CPU (mirroring arm64's `VBAR_EL1` setup of the same name) -- calls
  `gdt::initialize()` first, since the IDT's gates target a selector and,
  on a real fault from ring3, a TSS that file installs.
- `fault_address()`: read `%cr2`, the amd64 equivalent of arm64's `FAR_EL1`.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Interrupt gates, not trap gates (type 0x8E, not 0x8F): IF is cleared on
  entry, matching `common_handler`'s assumption that it runs with
  interrupts masked while saving registers and dispatching -- the same
  convention this codebase's arm64 exception path follows with DAIF masked.

## Verification

Verified by compilation, symbol survival under `--gc-sections` (confirmed
all 256 stubs, `common_handler`, and the vector table now link in, where
before this file's `initialize_idt()` existed they were unreferenced and
discarded), and disassembly review cross-checking the compiled IDT-building
loop's encoded gate bytes against this file's intended values. NOT verified
by execution: QEMU's multiboot loader only accepts a 32-bit kernel (see
`tools/run/run.sh`), so this architecture has not booted in this
environment, and this IDT has never actually received an interrupt.
