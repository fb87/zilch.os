# Hypervisor stage2

Production hypervisor module extracted during batch 0082. This module contains only the mechanism named by the file and does not contain certification profiles or modeled guest execution.

## Boundary

- Architecture-specific EL2 execution remains under `src/arch/*`.
- Userspace guest binaries remain under `src/user/guests/`.
- Bounded verification models remain under `tests/`.

Dynamic VMs own allocator-backed L1/L2/L3 tables. Mapping transactions rebuild
the bounded hierarchy, publish descriptors with a data barrier, invalidate the
VMID, and roll back on allocation failure. Unmap is excluded while vCPUs are
active. Conservative generation-tagged access/dirty state is available through
the capability ABI.
