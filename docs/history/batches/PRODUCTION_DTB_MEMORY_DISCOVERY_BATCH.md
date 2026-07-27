# Batch 0107 — DTB memory discovery and root inventory

## Scope

This batch replaces the fixed single QEMU RAM interval with a bounded parser for
the ARM64 Flattened Device Tree supplied in `x0` at boot. The kernel imports all
`memory` node `reg` tuples, the FDT reservation map, `/reserved-memory` child
ranges, the DTB blob itself, and the kernel image reservation.

The allocator builds up to sixteen page-aligned discontiguous allocatable
regions backed by one serialized bitmap. Allocation and release walk those
regions and preserve zero-on-allocation/reuse.

Bootinfo version 2 exports the resulting region inventory, page size, total page
count, and free page count to the PL3 root task. This is resource-discovery
metadata; frame allocation policy still remains in the kernel for now.

## Limits

- Supports one- or two-cell root address and size tuples.
- Metadata arrays are bounded.
- CPU discovery is not yet taken from DTB.
- Root receives inventory metadata, not untyped-memory capabilities.
- The userspace memory server does not yet import or subdivide this inventory.
