# Module: ARM64 hypervisor

## Purpose

Owns the ARM64 EL2 exception layer while leaving the L4 kernel at EL1.

## Initial scope

- Install one EL2 vector table on every CPU before entering EL1.
- Expose a small Zilch-specific HVC ABI.
- Preserve PSCI CPU startup through the platform SMC conduit.
- Diagnose unexpected EL2 exceptions without enabling guest virtualization yet.

## HVC ABI

The initial calls return the ABI version, current CPU ID, and an echoed test value.
Unknown calls return `-1`. Calls are handled entirely at EL2 and return to the
instruction following `hvc #0`.

## Invariants

- PSCI continues to use `smc #0`; HVC is reserved for Zilch.
- EL2 vectors are installed before any CPU enters EL1.
- EL2 does not trap normal EL1 timer, GIC, MMU, or scheduler operation.
- Assembly only saves/restores registers and routes to C++.
