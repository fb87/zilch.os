# Module: interrupt

## Purpose

Defines the interrupt module for the Zilch L4 microkernel.

## Responsibilities

- Provide a bounded, allocation-free implementation appropriate to its layer.
- Preserve the kernel/architecture/platform dependency boundary.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Public behavior is deterministic unless explicitly documented otherwise.

## Verification

The colocated `interrupt.tt` file is reserved for module-level tests.

## Redistributor discovery and bounded waits

The CPU-local redistributor is selected by matching `GICR_TYPER.Affinity_Value`
against `MPIDR_EL1`, rather than assuming redistributor frame order equals a
logical CPU number. Distributor and redistributor state transitions use bounded
polling and report an error instead of hanging indefinitely.

## Software-generated interrupts

The GICv3 CPU interface uses `ICC_SGI1R_EL1` for inter-processor interrupts.
SGI 0 is the reschedule IPI and SGI 1 is the TLB-shootdown IPI. Callers may
broadcast to every processing element except the sender or target one of the
four profile CPUs.

## Userspace interrupt inventory

Interrupt objects accept SPIs 32 through 1019. SGIs, PPIs, the architectural
virtual timer, and the spurious range are kernel-reserved and fail registration.
The exclusive kernel registry rejects a second owner for a live SPI.
