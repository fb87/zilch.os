# Versioned Contracts

Major contract generations use directories: `sys/arch/v1/` and `sys/platform/v1/`. Minor compatible changes append optional fields guarded by structure size and capability bits. Incompatible changes create `v2` without modifying `v1`.

Kernel-to-architecture calls use `ArchOps`; architecture-to-kernel entries use `KernelHooks`. Platform operations expose console, boot data, interrupt-controller integration, memory discovery, and firmware services.
