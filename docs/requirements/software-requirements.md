# Initial Software Requirements

- SSR-BOOT-001: Each supported target shall print a deterministic boot marker.
- SSR-ARCH-001: Generic kernel code shall depend only on `sys::arch::v1`.
- SSR-PLAT-001: Generic kernel code shall depend only on `sys::platform::v1`.
- SSR-TYPE-001: Physical addresses and object identifiers shall support 64-bit values.
- SSR-HYP-001: Hypervisor mechanisms shall be isolated behind the architecture contract.
- SSR-L4-001: Drivers and resource policy shall execute outside the privileged kernel.
