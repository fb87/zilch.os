# Module: hypervisor

## Purpose

Defines the hypervisor module for the Zilch L4 microkernel.

## Responsibilities

- Provide a bounded, allocation-free implementation appropriate to its layer.
- Preserve the kernel/architecture/platform dependency boundary.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Public behavior is deterministic unless explicitly documented otherwise.

## Verification

The colocated `hypervisor.tt` file is reserved for module-level tests.
