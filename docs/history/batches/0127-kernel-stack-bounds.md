# Batch 0127: kernel stack bounds and canaries

Each of the four physical CPUs now has independently tracked EL1 and EL2 stack
state:

- 32 KiB fixed bounds matching the linker and boot-assembly stride;
- eight 64-bit canary words at the bottom of each stack;
- an atomic lowest-observed stack pointer for retained high-water analysis.

The primary and secondary kernel entries seed both privilege-level stacks.
Every ARM64 exception dispatch validates the active stack pointer and canary
before resolving kernel objects or handling the exception. Corruption produces
a dedicated CPU/EL diagnostic and halts rather than continuing with a damaged
return path.

The full clean four-CPU certification suite passes with validation active,
including IPC lifecycle, capability transfer/revoke, object lookup/destroy,
pager, memory, hypervisor, fuzz, and reuse coverage.

This is deterministic detection, not isolation. Unmapped guard pages still
require splitting the current coarse kernel identity mapping.
