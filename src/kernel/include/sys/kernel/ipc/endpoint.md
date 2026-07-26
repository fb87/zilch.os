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
- a thread is pinned and cannot execute concurrently on two CPUs.

## Scope

The milestone supports register-only `call`, `receive`, and `reply_receive`.
Capability transfer, IPC buffers, cancellation, timeout, and migration are
intentionally deferred.

## Remote context ownership

A CPU shall not write another CPU's saved architectural exception frame.
Cross-CPU IPC publishes payloads to a per-thread pending mailbox, then marks the
thread ready. The owning CPU consumes the mailbox and updates the saved context
immediately before returning that thread to user mode. This prevents torn
register frames and preserves single-writer ownership of architecture state.
