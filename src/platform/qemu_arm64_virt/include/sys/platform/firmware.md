# Module: firmware

## Purpose

Defines the firmware module for the Zilch L4 microkernel.

## Responsibilities

- Provide a bounded, allocation-free implementation appropriate to its layer.
- Preserve the kernel/architecture/platform dependency boundary.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Public behavior is deterministic unless explicitly documented otherwise.

## Verification

The colocated `firmware.tt` file is reserved for module-level tests.

## PSCI conduit

QEMU `virt` direct kernel boot enters Zilch at EL2. In this configuration QEMU exposes PSCI 0.2 through the SMC conduit. `CPU_ON` therefore uses `smc #0`; using `hvc #0` would require an EL2 PSCI handler owned by Zilch.
