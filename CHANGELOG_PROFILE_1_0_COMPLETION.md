# Profile 1.0 completion candidate

- Added syscall 1 capability-authorized control plane.
- Added user ABI for capability lifecycle, thread control, mapping,
  notifications, interrupt binding, and scheduling-context configuration.
- Extended the raw syscall ABI to six arguments on ARM64 and AMD64.
- Added standalone root-server `init.elf` use of the frozen control ABI.
- Added Profile 1.0 acceptance contract and runtime gates.
- Bumped kernel version to 0.4.0.

The deterministic embedded ARM64 SMP image remains the runtime acceptance
profile until the earlyfs ELF-loader switch is validated on QEMU.
