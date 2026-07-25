---
module: sys.arch.arm64.boot.start
sources:
  - start.S
requirements:
  - ARCH-ARM64-BOOT-001
  - ARCH-ARM64-SMP-001
---

# ARM64 startup bridge

## Purpose

Provide the minimum assembly bridge required to enter the C++ kernel on the boot CPU and secondary CPUs.

## Responsibilities

- Select a dedicated stack from the CPU affinity ID.
- Normalize EL2 entry to EL1h.
- Initialize `SP_EL1` before returning to EL1h.
- Preserve the incoming EL2 vector table so firmware PSCI HVC remains available.
- Enable the GIC system-register interface for EL1.
- Clear BSS on the boot CPU only.
- Call the C++ boot or secondary entry point.

## Invariants

- C++ is never entered with an uninitialized stack.
- Secondary CPUs do not clear BSS.
- Both direct EL1 entry and EL2 entry reach the same EL1 C++ execution model.
- Zilch does not take ownership of EL2 until an EL2 monitor can forward PSCI calls.

## Non-responsibilities

Interrupt decoding, timer handling, GIC programming, SMP policy, and exception policy remain in C++.
