# Batch 0128: unmapped kernel stack guard pages

## Mapping change

The ARM64 kernel identity region no longer uses one coarse 1 GiB block. A
shared L2 table preserves 2 MiB identity blocks, while the linker-aligned
2 MiB stack window is split into L3 pages.

Every physical CPU receives:

- one 64 KiB EL1 slot with a 32 KiB stack;
- one 64 KiB EL2 slot with a 32 KiB stack;
- an unmapped 4 KiB page immediately below each usable stack.

All kernel and bounded user address-space roots reference the same protected
identity mapping, so switching TTBR0 cannot accidentally restore access to a
guard page.

## Verification

Kernel bootstrap checks:

- the stack-window L2 entry references the fine-grained L3 table;
- all eight EL1/EL2 guard entries are invalid;
- the page immediately above every guard remains mapped.

The existing exception-time bounds, canary, and low-water checks remain active.
The full clean four-CPU certification suite passes with the guarded mapping.

This completes production-readiness requirement SEC-004.
