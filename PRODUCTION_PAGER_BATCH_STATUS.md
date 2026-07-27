# Production Pager and Process Batch Status

> **Historical status notice:** This file records the state of its named batch. The current program status, completed follow-on work, remaining requirements, and delivery order are maintained in [`PROGRAM_CHECKLIST.md`](PROGRAM_CHECKLIST.md).

## Implemented

- Product ABI operations `process_create` and `process_destroy`, retaining the prior child operation values as compatibility aliases.
- Reusable task/thread/address-space/scheduling-context bundles backed by generation-safe dynamic object-table IDs.
- Selection of a free process slot independent of physical CPU ID.
- Transactional process construction with capability/object rollback on every failure path.
- Child CSpaces receive self, thread, address-space, scheduling-context and pager-endpoint capabilities.
- Global capability revocation during process destruction.
- Centralized fault-reply mapping primitive shared by the syscall path and certification tests.
- Fault resolution validates blocked-fault state, maps an allocator-owned frame, clears fault and timeout state, wakes the target, and requests remote rescheduling.
- Certification test covers dynamic process creation, blocked data fault, pager frame mapping, target resume, unmap, frame destruction, process destruction, and quota restoration.

## Verification state

- ARM64 certification build: PASS.
- ARM64 release build: PASS.
- ARM64 compatibility build: PASS.
- AMD64 compatibility build: PASS.
- Documentation and production gates: PASS.
- QEMU runtime certification: pending human `make BUILD_VARIANT=certification run`.

## Remaining production work

- Separate memory-server and client ELF images loaded by a userspace process server.
- True userspace pager receive/reply loop using fault IPC rather than the kernel certification helper.
- ELF loader and per-process image population.
- Dynamic endpoint and notification creation.
- Scalable process table beyond the current configured bound.
- Concurrent process create/destroy/fault races.
- Memory exhaustion, quota, and pager-failure policy tests.
