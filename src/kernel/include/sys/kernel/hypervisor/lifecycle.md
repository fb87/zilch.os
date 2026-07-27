# Hypervisor lifecycle

Production hypervisor module extracted during batch 0082. This module contains only the mechanism named by the file and does not contain certification profiles or modeled guest execution.

## Boundary

- Architecture-specific EL2 execution remains under `src/arch/*`.
- Userspace guest binaries remain under `src/user/guests/`.
- Bounded verification models remain under `tests/`.
