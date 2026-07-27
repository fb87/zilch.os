# Production Memory Objects and Pager Batch

> **Historical status notice:** This file records the state of its named batch. The current program status, completed follow-on work, remaining requirements, and delivery order are maintained in [`docs/roadmap/PROGRAM_CHECKLIST.md`](../../roadmap/PROGRAM_CHECKLIST.md).

## Implemented

- Generation-safe dynamic kernel object IDs from a 512-slot object table.
- Reusable pools for 64 frame objects and 32 page-table objects.
- Dynamic frame and page-table creation into a selected CSpace slot.
- Per-task page quotas and live ownership accounting.
- Destruction with capability revocation, object-table removal, zero-on-reuse,
  and quota rollback.
- Pager `fault_reply_map` operation that maps a delegated frame into the
  faulting thread, clears the fault record, resumes the thread, and requests a
  remote reschedule when necessary.
- Memory query and privileged quota configuration operations.

## Verification boundary

Build and certification coverage validate object creation, mapping, busy
rejection, unmapping, destruction, page-table lifecycle, quota accounting, and
allocator restoration. Runtime QEMU certification remains required.

## Still open

- Dynamically allocated task/thread/address-space objects.
- Unbounded/scalable metadata beyond configured production limits.
- Userspace memory-server executable and service supervision.
- Pager protocol integration test with a deliberately faulting client.
- Concurrent allocator and revoke stress under SMP.
- Memory-pressure and exhaustion certification across supported RAM sizes.
