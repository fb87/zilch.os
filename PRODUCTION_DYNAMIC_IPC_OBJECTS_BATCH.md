# Production dynamic IPC objects batch

> **Historical status notice:** This file records the state of its named batch. The current program status, completed follow-on work, remaining requirements, and delivery order are maintained in [`PROGRAM_CHECKLIST.md`](PROGRAM_CHECKLIST.md).

## Implemented

- ABI operations for endpoint and notification creation and destruction.
- Bounded, reusable kernel pools for dynamically created endpoint and notification objects.
- Generation-safe registration through the global object table.
- Transactional CSpace installation with rollback when registration or capability installation fails.
- Full owner capabilities with read, write, grant, and control rights.
- Global capability revocation before object-table removal.
- Busy rejection when destroying an endpoint with queued senders or a receiver.
- Busy rejection when destroying a notification with a registered waiter.
- Bootstrap endpoint and notification objects cannot be destroyed through the dynamic-object operations.
- Certification coverage for occupied-selector rejection, notification signal/poll, destruction, and generation-safe reuse.

## ABI additions

- `endpoint_create(selector)`
- `endpoint_destroy(selector)`
- `notification_create(selector)`
- `notification_destroy(selector)`

## Verification

- ARM64 certification build: PASS.
- QEMU runtime: blocked in the build environment because `qemu-system-aarch64` is not installed.
- Runtime evidence expected after running `make clean && make BUILD_VARIANT=certification run`:
  - `[TEST] name=dynamic_ipc_objects result=PASS`
  - final kernel and hypervisor acceptance PASS lines.

## Remaining work

- Use dynamically created endpoints and notifications in the process/pager service topology.
- Independently linked service ELF loading and per-process image population.
- Endpoint destruction policy that cancels blocked IPC participants when explicitly requested.
- Concurrent create/destroy and pool-exhaustion certification under SMP.
