# Module: timer

## Purpose

Defines the timer module for the Zilch L4 microkernel.

## Responsibilities

- Provide a bounded, allocation-free implementation appropriate to its layer.
- Preserve the kernel/architecture/platform dependency boundary.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Public behavior is deterministic unless explicitly documented otherwise.

## Verification

The colocated `timer.tt` file is reserved for module-level tests.

## SMP verification

Each CPU programs its own virtual timer before enabling IRQs. Per-CPU tick counters allow boot-time verification that the generic timer and GIC redistributor path operate on every online CPU.
