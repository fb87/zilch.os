# Production ELF instruction-cache coherency fix

Patch: 0078

## Problem

Reusing a user address-space slot could replace one ELF image with another at the
same virtual addresses and ASID. The bootstrap ELF loader copied new instructions
through the data cache but did not clean those lines to the point of unification
or invalidate corresponding instruction-cache lines. Secondary CPUs could
therefore execute stale instructions from the previous image.

Observed symptoms included lower-PL AArch64 synchronous exceptions with EC=0 at
valid instruction addresses after pager-client slots were reused for root-created
worker processes.

## Fix

After a successful ELF load, the ARM64 address-space backend now:

1. reads `CTR_EL0` to determine data and instruction cache-line sizes;
2. cleans the loaded image range with `dc cvau`;
3. executes `dsb ish`;
4. invalidates the image range with `ic ivau`;
5. executes `dsb ish` and `isb` before the image can run.

This provides the required data-to-instruction coherency for dynamically replaced
ELF images and virtual aliases.

## Verification

- ARM64 certification build
- ARM64 release build
- AMD64 release compatibility build
- Runtime regression requires the four-CPU certification profile and must show no
  unexpected `exception el=1 vector=8 esr=2000000` records during
  `root_created_objects` or `root_created_smp_fuzz`.
