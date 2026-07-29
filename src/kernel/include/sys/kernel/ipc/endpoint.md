# Module: kernel IPC endpoint

## Purpose

Provides fixed-capacity, capability-selected L4 endpoint rendezvous for the
pinned SMP user-scheduler bring-up.

## Responsibilities

- serialize endpoint state with a raw spinlock;
- queue blocked callers;
- match one blocked receiver with one caller;
- support cross-CPU wakeup through a reschedule SGI;
- provide bounded queue and structural invariant checks.

## Invariants

- sender count never exceeds the fixed capacity;
- head and tail always remain within the ring;
- at most one receiver waits on an endpoint in this milestone;
- each queued generation-checked thread reference is live and unique;
- a thread cannot occupy both a sender slot and the receiver slot;
- a thread is pinned and cannot execute concurrently on two CPUs.

## Scope

The production path supports register `call`, `receive`, `reply`,
`reply_receive`, cancellation, bounded timeouts, scheduling-context donation,
atomic batches of up to four capability transfers, and one-page frame-grant
out-of-line messages.

## Cancellation transaction

Cancellation follows the endpoint-to-IPC-lifecycle lock order. It snapshots
the blocked state and endpoint selector, locks that endpoint, then locks the
lifecycle transaction and revalidates both values before removing queue
membership or reply authority. This prevents a completed operation from being
mistaken for a later wait that happens to reuse the same thread state.

Dynamic endpoint destruction uses a separate retirement transaction. It locks
the endpoint, acquires capability authority, revalidates the exact control
capability, verifies the queue is empty, and publishes `retiring` before
revoking every capability. Call and receive recheck retirement after acquiring
the endpoint lock, covering operations that resolved the object immediately
before revoke. Authority and the endpoint lock are released before unregister
waits for pre-existing object readers.

## Remote context ownership

A CPU shall not write another CPU's saved architectural exception frame.
Cross-CPU IPC publishes payloads to a per-thread pending mailbox, then marks the
thread ready and sends a reschedule SGI only to its pinned CPU. The owning CPU
consumes the mailbox and updates the saved context immediately before returning
that thread to user mode. This prevents torn register frames, preserves
single-writer ownership of architecture state, and avoids broadcast wakeups.
