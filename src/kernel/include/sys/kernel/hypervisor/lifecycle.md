# Hypervisor lifecycle

Production hypervisor module extracted during batch 0082. This module contains only the mechanism named by the file and does not contain certification profiles or modeled guest execution.

## Boundary

- Architecture-specific EL2 execution remains under `src/arch/*`.
- Userspace guest binaries remain under `src/user/guests/`.
- Bounded verification models remain under `tests/`.

## Dynamic objects

Four dynamic VM slots and eight dynamic vCPU slots provide a bounded production
pool. Every VM receives a VMID generation and a dedicated page-aligned stage-2
root. VM/vCPU headers are registered in the generation-checked object table and
installed only through capabilities.

Destroy rejects active execution, attached vCPUs, and live mappings. It revokes
authority, unregisters the exact object generation, releases the VMID, scrubs
vCPU architectural state and stage-2 root storage, and only then permits reuse.
The pool lock precedes capability, VMID, and object-table ranks.
