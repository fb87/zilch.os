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

The platform accepts absolute deadlines in the kernel's 100 Hz logical
timebase. It converts the requested delta to `CNTV_TVAL_EL0`, clamps it to the
architectural signed interval limit, and records the actual programmed delta.
On interrupt, logical time advances by that delta rather than by one, preserving
timeout ordering across tickless idle intervals. Active schedulers request the
next quantum; idle schedulers request the earliest timeout or a bounded
housekeeping deadline.

`CNTFRQ_EL0` must produce a nonzero 100 Hz interval within the architectural
timer bound. Invalid frequencies fail closed to the minimum interval.
Absolute deadline addition saturates at `UINT64_MAX`; it never wraps a timeout
into the past. The logical counter itself also saturates at that limit.

Final certification requires every online CPU to have nonzero timer progress
and a programmed delta within the frequency-derived architectural bound.
