# Architecture

Zilch follows the classic L4 principle: the privileged kernel contains only address spaces, threads, scheduling contexts, capabilities, synchronous IPC, interrupt delivery, and fault delivery. Device drivers, filesystems, networking, loaders, and resource policy execute in user space.

## Layering

1. Generic kernel mechanisms.
2. `sys::arch::v1`: versioned CPU architecture contract.
3. `sys::platform::v1`: versioned machine/board contract.
4. Architecture backend: ARM64 or AMD64.
5. Platform backend: QEMU ARM64 virt or QEMU AMD64 q35.

Architecture and platform contracts never share native headers with generic kernel code. Native headers live under `sys/arch/native` and `sys/platform/native`.

## Privilege abstraction

- PL0 Monitor
- PL1 Hypervisor
- PL2 Kernel
- PL3 User

These are architecture-neutral design roles. ARM exception levels and AMD64 privilege/VMX details remain backend-private.
