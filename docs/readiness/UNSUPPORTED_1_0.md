# Explicitly unsupported in the 1.0 kernel baseline

The following are intentionally unsupported rather than silently accepted:

- CPU hotplug and CPU offline after boot. The supported CPU set is immutable
  after SMP initialization; firmware must present all CPUs before kernel start.
- System suspend/resume. Monotonic timer and scheduler state are valid only
  during one uninterrupted boot.
- AMD64 runtime and virtualization. AMD64 remains compile-only; its
  virtualization backend is deferred beyond 1.0.
- Loadable kernel modules and runtime kernel text patching.
- Direct device assignment and DMA before SMMU ownership, quiescence, and reset
  support is complete.
- Hardware platforms other than the explicitly certified ARM64 platform list.
- In-kernel watchdog, reset, and power-off control for the QEMU ARM64 profile;
  the external machine controller owns termination and restart.
- UART transfer to a userspace driver in the root-only profile; the polling
  UART remains a kernel diagnostic sink.

Unsupported operations must return `unsupported` where an ABI entry exists.
They must never partially mutate state. Adding support requires implementation,
negative tests, runtime certification, and removal from this document in the
same release.
