# 0108 — DTB discovery fallback fix

## Problem

The first DTB-backed allocator build halted before user-object initialization with
`not_found`. Some QEMU raw-kernel boot configurations did not provide a usable
firmware-data pointer in `x0`, so the bounded FDT parser had no input.

## Resolution

Physical-memory discovery now uses an explicit ordered policy:

1. parse the firmware pointer supplied in `x0`;
2. on ARM64 QEMU virt, probe the conventional DTB location at the RAM base;
3. if neither contains a valid FDT, use the platform RAM description as an
   explicit fallback rather than halting.

The selected source is printed in the memory inventory log as
`firmware-register`, `platform-probe`, or `platform-fallback`.

The fallback does not claim firmware discovery completion. It exists so the
kernel remains bootable while retaining visible evidence of which inventory
source was used.
