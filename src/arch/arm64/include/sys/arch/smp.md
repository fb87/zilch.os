# Module: ARM64 SMP

## Purpose

Boots QEMU `virt` secondary CPUs through PSCI `CPU_ON` and tracks online CPUs.

## Invariants

- CPU 0 clears BSS and initializes global interrupt-controller state.
- Secondary CPUs never clear shared BSS.
- Each supported CPU has a distinct 16 KiB bootstrap stack.
- A CPU publishes its online bit only after its EL1 vector and GIC CPU interface are installed.

## IPI bring-up

SGI 0 is reserved for reschedule requests and SGI 1 for TLB shootdown requests. The boot CPU verifies SGI delivery after all configured CPUs become online. Per-CPU counters are bring-up instrumentation and will later be replaced by scheduler and address-space callbacks.

All online waits are bounded. A missing secondary CPU, IPI, or timer interrupt is reported as a timeout rather than causing an unbounded boot spin.
