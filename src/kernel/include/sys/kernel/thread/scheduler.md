# Module: kernel thread scheduler

## Purpose

Provides pinned per-CPU user-thread scheduling for the first SMP userspace
milestone.

## Design

- ten user threads are distributed round-robin across four CPUs;
- each CPU owns its current-thread index and scheduling progress counter;
- timer IRQs and reschedule SGIs rotate only the local pinned run set;
- blocked CPUs return to an EL1 idle context until a remote wakeup or timer IRQ;
- no migration or work stealing is permitted in this milestone;
- user execution starts only after SMP, timer, IPI, hypervisor, and TLB checks.

## Invariants

- one thread may be current on at most one CPU;
- a thread's pinned CPU never changes;
- remote state transitions use release/acquire publication;
- a CPU with no runnable user thread executes the kernel idle context.


## Control-state integrity

Before returning to user mode, the scheduler validates that the saved program
counter, stack pointer, and execution mode describe the configured user image.
Invalid contexts are fault-contained instead of being passed to `eret`. EL1 idle
frames are rebuilt from zero rather than derived from interrupted user frames.

## Strict EL1 idle provenance

An IRQ may replace its exception frame with a user context only when all of the
following are true:

- the ARM64 vector is current-EL using SPx IRQ (vector 5);
- the saved `ELR_EL1` equals `sys_kernel_user_idle`;
- the saved `SPSR_EL1` selects EL1h;
- the per-CPU scheduler is marked idle; and
- a pinned user thread is runnable.

IRQs interrupting syscall handling, fault containment, logging, endpoint code,
or any other EL1 path return to the interrupted kernel instruction unchanged.
This prevents a nested timer or reschedule SGI from converting an arbitrary EL1
exception frame into an EL0 return frame.

## Boot diagnostics

Before releasing the user-scheduler launch gate, the boot CPU prints the
assignment derived from every thread's `pinned_cpu` field. The table is therefore
the scheduler's actual configuration rather than a separately maintained
example. With ten threads and four CPUs, the expected output is:

```text
[INFO] user scheduler pinning table:
[INFO] user scheduler: cpu=0 threads=[0,4,8]
[INFO] user scheduler: cpu=1 threads=[1,5,9]
[INFO] user scheduler: cpu=2 threads=[2,6]
[INFO] user scheduler: cpu=3 threads=[3,7]
```
