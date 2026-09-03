# Module: timer (amd64)

## Purpose

Programs the LAPIC's own periodic timer to fire real interrupts at
`ticks_per_second` through vector `interrupt::virtual_timer_irq` (32) --
delivered via the real IDT `sys::arch::exception::initialize_idt()`
installs. Before this file measured anything, the vector table those
interrupts would target was itself unreferenced dead code (see
`exception.md`), so there was nothing here to wire up until that existed.

## Responsibilities

- `calibrate_tsc()`: establish `tsc_frequency` from CPUID leaf 0x15
  (TSC/core-crystal-clock ratio) where enumerated, falling back to leaf
  0x16's processor base frequency, falling back further to a fixed
  conservative estimate if the CPU model reports neither.
- `calibrate_lapic()`: measure the LAPIC's own countdown-clock rate against
  the TSC -- program a large one-shot count, busy-wait a TSC-measured
  interval, see how far it counted down. This codebase has no other
  absolute time reference on this platform (no PIT/HPET wired up), so the
  TSC is the only thing available to calibrate against; the LAPIC's bus
  clock rate under QEMU is not a value safe to assume as a constant.
- `start_periodic()`: program vector 32, periodic mode, and the Initial
  Count computed from the calibrated rate, then leave it running --
  periodic mode auto-reloads on every fire, so nothing needs to re-arm it
  per interrupt the way arm64's one-shot model does.
- `handle_interrupt()`: increments a real per-tick counter. Nothing reads
  its return value yet (`arch.cc`'s dispatch discards it with `(void)`);
  it replaces a `1000000U` placeholder that no periodic interrupt had ever
  actually produced.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- `ticks(cpu)` deliberately still reads the TSC directly rather than
  deriving from the new interrupt-driven counter above. It is called from
  several already-live sites (`kernel.hh`'s boot readiness check,
  `kernel/interrupt.hh`'s delivery timestamping, `thread/scheduler.hh`'s
  scheduling-context accounting) this change did not set out to touch, and
  changing its semantics would affect all of them simultaneously in an
  environment where amd64 cannot be booted to verify the result. Wiring the
  interrupt itself is this change's scope; re-deriving `ticks()` from it is
  a separate, later step.

## Verification

Not verified by execution -- QEMU's multiboot loader only accepts a 32-bit
kernel (see `tools/run/run.sh`), so this has never actually fired an
interrupt. Verified by compilation and exhaustive disassembly cross-check:
every LAPIC register write (Divide Configuration at `+0x3e0`, LVT Timer at
`+0x320`, Initial Count at `+0x380`, Current Count read at `+0x390`) and
every computed value (the calibration window, the busy-wait bound, the
`elapsed == 0` fallback, the final periodic Initial Count) was traced in
the compiled machine code and matches this file's intended arithmetic,
including the compiler's own independently-derived division-by-100
strength reduction -- not merely re-reading the source that produced it.
