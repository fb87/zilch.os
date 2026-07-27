# Zilch ARM64 Kernel Profile 1.0 Integration

This batch integrates checkpoints K2 through K6 while retaining the deterministic SMP compatibility workload as the boot-time regression profile.

## K2 — capability and object lifecycle

- bounded CSpace copy, move and delete
- grant-right enforcement and rights attenuation
- generation-safe endpoint sender/receiver references
- endpoint cancellation and destruction hooks
- task, thread, address-space and scheduling-context capabilities

## K3 — IPC and fault delivery

- synchronous call, receive and reply-receive remain capability-authorized
- reply authority remains one-shot and generation-bound
- EL0 faults are converted to synchronous fault IPC
- faulting threads block in `blocked_fault`
- pager replies select resume or terminate
- compatibility root pager replies terminate for the deliberate null-fault test

## K4 — memory and IRQ object foundation

- typed frame and page-table objects
- capability-ready map/unmap kernel API
- W+X mapping rejection
- ASID invalidation after map/unmap
- notification object with badge accumulation
- interrupt object bound to a notification reference

## K5 — root-task bootstrap contract

- versioned `bootinfo`
- task 0 designated as the compatibility root task
- initial Task, Thread, AddressSpace, SchedulingContext, Endpoint, Frame,
  Notification and Interrupt capabilities
- root pager endpoint selector recorded in bootinfo

The current ARM64 user image remains the deterministic compatibility root/fuzz
image. Replacing it with a standalone root server ELF is the next userspace
transition, not a kernel-ABI redesign.

## K6 — stabilization

- boot-time capability lifecycle self-test
- boot-time frame map/unmap self-test
- boot-time notification signal/consume self-test
- full ARM64 and AMD64 warning-clean builds
- documentation coverage check
- existing per-CPU fuzz health reporting retained
