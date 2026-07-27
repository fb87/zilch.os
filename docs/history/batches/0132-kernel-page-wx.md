# Batch 0132: kernel page-granular W^X

## Targets

- Complete SEC-001 kernel and guest W^X.
- Complete SEC-002 W^X across production address spaces.
- Complete SEC-003 kernel read-only-data protection.

## Implementation

The linker now exports page-aligned boundaries for kernel text, embedded images/rodata, and writable data/BSS. The two 2 MiB identity blocks containing the kernel image are replaced with shared L3 tables:

- kernel text: EL1 read-only, executable, EL0 execute-never;
- embedded user images and kernel rodata: EL1 read-only, PXN and UXN;
- data, BSS, page tables, and other mutable identity pages: EL1 read-write, PXN and UXN.

The permanent kernel root and every user TTBR0 root reference the same protected identity tables. Existing user and stage-2 mapping APIs independently reject writable-executable permissions.

## Evidence

Bootstrap certification walks all 1,024 image-window PTEs, validates each linker region's expected permission class, and rejects any writable-executable descriptor. The clean four-CPU certification suite passes user W^X rejection, stage-2 W^X rejection, real guest execution, SMP lifecycle races, and final root-only acceptance with zero failures.
