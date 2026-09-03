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
- `handle_interrupt()`: increments a real per-tick counter, read by
  `ticks()` below. It replaces a `1000000U` placeholder that no periodic
  interrupt had ever actually produced.
- `ticks(cpu)`: reads that same counter -- no longer TSC-derived. This is
  now the same kind of value arm64's `ticks()` has always been: a count of
  real timer interrupts, not a continuously-sampled clock. That parity
  matters beyond consistency for its own sake -- `kernel/interrupt.hh`'s
  storm-detection window (`storm_window_ticks = 100` at
  `ticks_per_second = 100`) is portable code shared by both architectures,
  and it is written assuming exactly this semantic (100 ticks = one real
  second of real interrupts). amd64's previous TSC-based version
  numerically advanced at a similar rate for a different reason -- it
  advanced continuously regardless of whether any interrupt had ever
  fired -- which made `kernel.hh`'s boot readiness check
  (`ticks(cpu_id) == 0U` meaning "not yet online") vacuous: it read
  nonzero from the very first read, boot or no boot, timer or no timer.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.

## A separate, larger bug this surfaced

Attempting to verify the boot readiness loop above would actually observe
a real tick found that it never runs at all: `kernel.hh`'s `start()` calls
`arch::smp::boot_secondary_cpus()` unconditionally and halts on anything
but `error_t::success`. amd64's implementation (`arch/amd64/include/sys/
arch/smp.hh`) returns `error_t::unsupported` unconditionally, where
arm64's equivalent returns `success` for a 1-CPU count (its loop over
secondary CPUs does zero iterations and falls through). With this
platform's `boot_info.cpu_count` hardcoded to 1
(`platform/firmware.hh`), amd64 has been halting immediately after
printing `smp: boot CPU online` on every boot -- before ever reaching the
SMP-count, IPI, or timer-readiness checks this file's own calibration work
exists to satisfy.

That stub looked like a one-line fix (return `success` when
`cpu_count <= 1`, matching arm64's contract), but changing it exposed a
real compilation error in previously-unreached code: `kernel/memory/
manager.hh`, reached via `kernel/hypervisor/stage2.hh` further down the
same boot function, contains an ARM-specific `dsb ishst` instruction that
had never actually been compiled for amd64's execution path before --
only for its *reachable-from-`start()`-today* subset, which excluded
everything past the halt. Fixing the halt is small; finding out how much
else downstream was never really compiled for amd64 is not, and is a
separate, larger task from wiring one timer interrupt. The one-line
`smp.hh` fix was reverted rather than pursued here for that reason -- see
this project's history for whether and when it was picked up.

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
