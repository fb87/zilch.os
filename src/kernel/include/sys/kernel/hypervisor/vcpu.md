# Hypervisor vcpu

Production hypervisor module extracted during batch 0082. This module contains only the mechanism named by the file and does not contain certification profiles or modeled guest execution.

## Boundary

- Architecture-specific EL2 execution remains under `src/arch/*`.
- Userspace guest binaries remain under `src/user/guests/`.
- Bounded verification models remain under `tests/`.

The saved context covers all general registers, PC/PSTATE, EL1 translation and
exception state, TLS, timer state, virtual-GIC control/AP/list registers, and
the per-vCPU hypercall report accumulator. Entry sanitizes guest-controlled
registers. Per-VM transactions account entry/exit while allowing independent
vCPUs to execute concurrently on different physical CPUs.
