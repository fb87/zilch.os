# Batch 0129: reuse-state scrubbing

## Production requirement

SEC-006 requires memory and architectural state to be zeroed before ownership or object slots are reused.

## Implementation

- Physical pages are zeroed at both allocation and release boundaries.
- Bootstrap certification writes a page-wide poison pattern, releases the page, immediately reallocates it, and verifies the same page contains only zeroes.
- User-thread teardown clears saved registers, IPC messages and reply state, scheduling state, fault records, counters, and execution metadata.
- vCPU teardown uses volatile byte stores to scrub all general and system registers, virtual timer and interrupt state, exit records, and diagnostic records without relying on a freestanding `memset`.
- Device MMIO frames are excluded because writing zeroes to device registers is unsafe; device reset policy remains tracked by DEV-004 and DEV-017.

## Evidence

`make BUILD_VARIANT=certification format clean run` completed the four-CPU root-only suite with `failures=0`. Hypervisor teardown tests poison register, system-register, timer, interrupt, exit, and diagnostic fields before verifying they are zero.
