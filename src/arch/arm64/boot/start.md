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

- Select distinct per-CPU kernel and hypervisor stacks from the CPU affinity ID.
- Normalize EL2 entry to EL1h.
- Initialize `SP_EL1` with the kernel stack before returning to EL1h.
- Keep `SP_EL2` on a separate hypervisor stack so EL2 exceptions cannot overwrite live EL1 frames.
- Install Zilch EL2 vectors while preserving firmware PSCI through the SMC conduit.
- Enable the GIC system-register interface for EL1.
- Clear BSS on the boot CPU only.
- Call the C++ boot or secondary entry point.

## Invariants

- C++ is never entered with an uninitialized stack.
- Secondary CPUs do not clear BSS.
- Both direct EL1 entry and EL2 entry reach the same EL1 C++ execution model.
- EL1 and EL2 stack ranges never overlap.
- PSCI remains on SMC; HVC is reserved for the Zilch hypervisor ABI.

## Non-responsibilities

Interrupt decoding, timer handling, GIC programming, SMP policy, and exception policy remain in C++.
