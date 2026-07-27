# Batch 0122: IPC lifecycle serialization

## Scope

Harden the production boundary shared by IPC reply, explicit cancellation,
timeout expiry, thread exit, and process teardown.

## Mechanism

A kernel IPC-lifecycle lock serializes blocked-state ownership, pending syscall
results, capability transfers, and single-use reply authority. Endpoint queues
are locked before the lifecycle lock. Timer expiry uses a non-blocking claim so
IRQ context cannot spin behind an interrupted syscall; contention is retried on
the next tick.

Queue removal now requests write authority for blocked senders and read
authority for blocked receivers. Error-only wakeups restore the pending error
to the syscall result even when no IPC message accompanies the completion.

Exiting servers revoke donation and resolve a live caller deterministically:
ordinary callers receive `timed_out`, while unresolved fault callers terminate
instead of resuming without pager policy.

## Evidence

`ipc_lifecycle_races` covers:

- explicit cancellation of a write-only blocked caller;
- timer expiry and endpoint queue removal;
- destruction of a blocked caller followed by endpoint reuse;
- server exit with live reply authority;
- subsequent pager and memory-server protocol execution on the same endpoint.

## Limitations

The lock is a correctness-first global serialization boundary. Scalable
per-endpoint/per-thread locking, controlled instruction-level interleaving,
multi-capability transfer races, and long-duration SMP race fuzz remain open.
