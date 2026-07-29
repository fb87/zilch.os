# Hypervisor virtual timer

The production timer state mirrors the guest compare and control registers at
each vCPU boundary. It retains a single pending expiry until acknowledgement,
counts lifecycle transitions, and detects an elapsed deadline against the
VM-relative architectural counter.

Each virtual machine owns a counter offset. The ARM64 EL2 entry path programs
that value into `CNTVOFF_EL2`, verifies it, and restores the host value after
the guest leaves.

## Boundary

- Architecture-specific EL2 execution remains under `src/arch/*`.
- Userspace guest binaries remain under `src/user/guests/`.
- Bounded verification models remain under `tests/`.
- Dynamic vCPU deadlines participate in per-CPU host deadline programming.
  Timer interrupt polling queues a single virtual event and transitions a
  blocked vCPU back to runnable before its next entry.
