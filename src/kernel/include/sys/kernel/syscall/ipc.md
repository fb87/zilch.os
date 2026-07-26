# Module: IPC syscall dispatch

## Purpose

Implements the minimal L4-style `sys_ipc` entry and the deterministic SMP fuzz
workload.

## Operations

- `call`: send four message registers and block for a reply;
- `receive`: block until a caller rendezvous occurs;
- `reply_receive`: reply to the previous caller and atomically wait again.

The implementation uses two fixed endpoint capabilities and register-only
messages. Servers are pinned to CPUs 0 and 1 while clients execute on all CPUs.
Remote wakeups request rescheduling through SGI.

## Fuzzing

Malformed capability selectors, operations, identities, and boundary values are
validated in the same syscall path with stable per-thread seeds. Progress and
failure counters are atomic and therefore meaningful under SMP.

## Deferred

Capability transfer, IPC buffers, cancellation, timeout, endpoint destruction,
and thread migration are not part of this milestone.


## Fuzz discriminator lifetime

The deterministic test harness uses `x6 == fuzz_magic` only for a single
validation call. The kernel clears the saved `x6` before returning, and normal
user IPC stubs clear `x6` before `svc #0`. This prevents a fuzz discriminator
from leaking into the next ordinary endpoint call.
