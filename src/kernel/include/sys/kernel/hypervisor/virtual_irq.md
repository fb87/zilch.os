# Hypervisor virtual irq

Production hypervisor module extracted during batch 0082. This module contains only the mechanism named by the file and does not contain certification profiles or modeled guest execution.

## Boundary

- Architecture-specific EL2 execution remains under `src/arch/*`.
- Userspace guest binaries remain under `src/user/guests/`.
- Bounded verification models remain under `tests/`.

The production controller classifies SGIs, PPIs, and SPIs and implements PMR
and per-source masking, deterministic priority selection, edge/level behavior,
and pending/active/deactivate/re-pend transitions. ARM64 uses ICH list
registers where available and handles the maintenance PPI; the HCR virtual-IRQ
path provides the same guest acknowledgement contract as a fallback.
