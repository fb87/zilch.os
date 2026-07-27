# Scheduler claim versus teardown race fix

Batch: 0093

## Failure

A secondary CPU could observe a thread as runnable, then teardown could change it to `suspended` and observe `executing=false`. The scheduler subsequently performed an unconditional store to `running`, resurrecting the thread after teardown had begun. This allowed execution from a reclaimed or replaced user image and produced delayed EL1 undefined-instruction exceptions at otherwise valid user virtual addresses.

## Correction

- Added an atomic thread-state compare/exchange helper.
- Scheduler dispatch publishes `executing=true` before attempting `ready -> running`.
- Dispatch proceeds only when the compare/exchange succeeds; otherwise the execution claim is withdrawn.
- Descheduling uses conditional `running -> ready` rather than an unconditional store.
- Fault resolution uses conditional `blocked_fault -> ready`.
- Initial PL3 entry uses the same execution-claim protocol.
- IPC enqueue rollback uses conditional `blocked_send -> running`.

## Invariant

A process bundle may be reclaimed only when no CPU owns or is committed to a return into its PL3 context. A scheduler cannot transition a thread from `suspended` or `terminated` back to `running`.

## Evidence

Compile-time gates pass for ARM64 certification, ARM64 release, and AMD64 compile-only release. Runtime four-CPU certification remains required to close the reported regression.
