# Batch 0175: portable host verification

## Added coverage

`tests/host/kernel_logic.cc` includes the production capability and scheduling
headers directly. It exhausts all bounded rights masks and executes 65,536
deterministic scheduling operations while checking consumption, replenishment
capacity/order, donation depth, priority inheritance, and unwind.

The host runner enables AddressSanitizer, UndefinedBehaviorSanitizer,
non-recovering failures, LLVM instrumentation coverage, and the Clang analyzer
profile through clang-tidy with warnings as errors. CI installs and executes
the same tools. Coverage is emitted to
`out/reports/host-kernel-coverage.txt`.

## Scope

This closes the portable capability/scheduling host-test gap. It does not claim
host emulation of architecture-specific IPC, VM, stage-2, MMIO, or exception
paths; those remain covered by cross-compilation and runtime certification.

The overall verification/soak gate remains open for real userspace/device
integration, missing IPC/VM/stage-2 host units, retained 24/72-hour runs,
repeated reboot evidence, and the real-hardware qualification matrix.
