# Module: ARM64 address spaces

## Purpose

Builds private user address spaces and activates them through `TTBR0_EL1`.

## Design

- every user thread owns a private root and stack page;
- the user code page is shared read-only;
- each address space receives a unique nonzero ASID;
- activation records an active-CPU mask and writes the ASID-tagged TTBR0;
- the architecture backend provides ASID invalidation and global TLB fallback;
- threads are pinned, so address-space migration is not yet required.
