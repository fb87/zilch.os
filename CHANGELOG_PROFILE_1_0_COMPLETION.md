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

## 0.6.0 final root-created certification

- Added root-authorized child bundle creation and destruction.
- Added dynamic root-created workers pinned to CPUs 1-3.
- Added deterministic negative control-ABI fuzzing on each secondary CPU.
- Added suspend, teardown, object generation reuse, and stale-capability tests.
- Added machine-readable root-created object, SMP fuzz, and lifecycle verdicts.

## Continuous soak fuzz mode

- Added `FUZZ_MODE=certify|soak`.
- Added `make run-certify` and `make run-soak`.
- Soak mode starts only after finite Profile 1.0 acceptance passes.
- Root recreates workers on CPUs 1-3 and runs unbounded deterministic fuzzing.
- Per-epoch logs include each CPU's operation count, failure count, and last
  replayable PRNG state.
