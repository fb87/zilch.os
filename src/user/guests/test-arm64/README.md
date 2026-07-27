# ARM64 hypervisor verification guest

This directory owns the independently linked ARM64 guest executable used by
certification. It is guest software, not kernel or architecture boot code.

The guest uses only architectural registers and the guest-visible HVC contract.
It must not include private kernel headers. Certification packages
`guest-test.elf` as `/guests/test-arm64.elf` in earlyfs.

Until the PL3 domain manager loads guest ELF files, a certification-only blob
adapter exposes the raw guest image to the existing real-single-vCPU harness.
That adapter is excluded from release builds and is not the production loading
architecture.
