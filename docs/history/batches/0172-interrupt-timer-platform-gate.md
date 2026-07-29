# Batch 0172: interrupt, timer, and platform completion gate

## Supported profile

This batch completes the versioned QEMU ARM64 virt profile. The profile has
four immutable CPUs, GICv3, architectural virtual timers, SGIs 0/1 for kernel
reschedule and TLB shootdown, PPI 27 for the timer, and userspace-assignable
SPIs 32 through 1019.

Real hardware qualification remains an independent release gate. AMD64 remains
compile-only. Watchdog, kernel reset/power-off, and UART transfer to a driver
are explicitly outside the root-only QEMU profile.

## Completion evidence

- Interrupt registration rejects private/reserved lines and duplicate owners.
- Edge SPI 40 and level SPI 41 traverse configure, bind, dispatch,
  notification, priority-drop/deactivate, and acknowledge.
- Guarded cross-CSpace delegation attenuates rights and revoke removes the
  descendant.
- Storm containment masks after the 64-event window.
- Final acceptance validates the empty/live IRQ registry, fixed platform
  inventory, timer frequency/interval constraints, and nonzero bounded timer
  state for all four online CPUs.
- `interrupt_timer_platform_gate` aggregates timer, IPI, lifecycle, SMP,
  teardown, and reuse workloads.

## Verification

- `make format-check abi-check boundary-check`
- `make BUILD_VARIANT=certification run`
- `make production-gate`
- AMD64 compile-only release ELF, section-permission, and stack-usage checks
