# Thread-exit notification batch 0121

## Scope

Close the completion-versus-teardown race in the userspace memory-server
pressure clients.

## Mechanism

`thread_exit` accepts an optional notification selector and badge. The kernel
publishes the terminal thread state, validates notification authority, signals
the supervisor, and commits another user context or kernel idle without
returning to PL3.

The memory clients use this atomic operation instead of separate
`notification_signal` and `thread_exit` syscalls.

## Limitations

This is not yet a general process wait/status API. Exit status collection,
parent/supervisor policy, and crash reporting remain open.
