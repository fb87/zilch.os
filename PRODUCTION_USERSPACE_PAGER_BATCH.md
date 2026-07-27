# Production userspace pager batch

> **Historical status notice:** This file records the state of its named batch. The current program status, completed follow-on work, remaining requirements, and delivery order are maintained in [`PROGRAM_CHECKLIST.md`](PROGRAM_CHECKLIST.md).

## Implemented

- Separate PL3 memory-server and fault-client processes launched by the root task.
- Role-selected service entry paths in the embedded native userspace image.
- Real client data abort at `0x20004000`.
- Fault IPC rendezvous with a userspace memory server.
- Server-owned dynamic frame allocation.
- Reply-authority based fault resolution without requiring a target-thread capability.
- Client retry and read/write verification after mapping.
- Client completion IPC, mapping reclaim, frame destruction, and process teardown.
- Notification-based root synchronization.
- Product ABI operations `fault_reply_sender` and `pager_reclaim_sender`.
- Userspace syscall helpers that return IPC message registers and secondary syscall results.

## Runtime evidence required

Run `make clean && make BUILD_VARIANT=certification run` and retain:

- `[TEST] name=userspace_pager_service result=PASS`
- `[TEST] name=pager_fault_reply result=PASS`
- `[TEST] name=dynamic_memory_objects result=PASS`
- final kernel and hypervisor acceptance PASS lines.

## Remaining userspace work

- Build and load independently linked service ELF files instead of role paths in one image.
- General ELF loader and image population.
- Dynamic endpoint and notification object creation.
- Pager refusal/termination policy and server-death recovery.
- Multiple concurrent clients and memory-pressure tests.
