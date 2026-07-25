# Boot-to-userspace design

1. Architecture startup establishes the kernel execution environment.
2. Platform initialization exposes early console and normalized boot data.
3. The kernel initializes L4 objects, capabilities, scheduler, MMU, and IRQs.
4. The kernel locates earlyfs and validates its manifest.
5. The kernel creates the root address space, capability space, and thread.
6. The kernel maps `/bin/init`, its stack, boot information, and initial caps.
7. The kernel enters PL3 at the userspace entry point.
8. `init` starts essential servers and later mounts the real root filesystem.

Only steps 1-3 are implemented in the current skeleton. The build already emits
`init.elf` and `earlyfs.tar` so steps 4-7 can be developed against stable inputs.
