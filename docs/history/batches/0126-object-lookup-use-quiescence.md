# Batch 0126: object lookup/use quiescence

## Problem

A capability lookup could return a coherent, generation-checked object pointer,
then a concurrent destroy could unregister and reuse the backing object before
the first syscall finished using that pointer.

## Protocol

ARM64 exception dispatch now enters a per-CPU object read-side section before
any syscall, fault, interrupt, or hypervisor object resolution and leaves it on
return.

Object unregister:

1. atomically removes the generation-tagged table entry, preventing new
   readers from resolving it;
2. waits for pre-existing read-side sections on every other CPU;
3. returns only when the backing object can be safely cleared or reused.

The destroying CPU is excluded from the wait because destruction itself runs
inside a read-side section and synchronously owns its local references.

## Certification

`object_lookup_destroy_race` delegates a dynamic notification to a worker on
CPU 1. The worker repeatedly resolves and signals it while root on CPU 0
destroys the notification. The worker must observe revoked authority and exit,
after which process and selector reuse continue normally.

The complete downstream pager, memory, SMP fuzz, and object destroy/reuse suite
passes after the race.

The wider exception path exposed insufficient headroom in the 16 KiB EL1
stacks during race-heavy teardown. EL1 and EL2 stacks are now separate 32 KiB
per-CPU regions, and the boot stack selector uses the corresponding stride.
Guard pages and measured high-water enforcement remain open.

## Remaining work

- replace the fixed global CPU scan with a scalable reclamation strategy;
- audit and annotate any future object readers outside exception dispatch;
- add stack guard pages and retained high-water evidence;
- add long-duration generation-wrap and forced-interleaving stress.
