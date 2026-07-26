# Production Kernel IPC Batch

Status: implementation complete, QEMU runtime certification pending.

This cumulative production branch upgrades the native ARM64 IPC path with:

- one-shot reply tokens carrying caller generation and nonce;
- bounded scheduling-context priority donation and rollback on reply/cancel/timeout;
- atomic capability mint into the receiver CSpace during rendezvous;
- rollback-by-construction: the receiver is not woken and the sender is not committed when transfer fails;
- bounded IPC timeouts driven by per-CPU timer ticks;
- privileged cancellation of blocked send, receive, or reply operations;
- endpoint queue removal and stale reply invalidation during cancellation;
- cross-CPU wakeup after cancellation, reply, or timeout;
- packed ARM64 ABI descriptors in x6 (capability transfer) and x7 (timeout), with x8 retained as syscall number;
- an ARM64 userspace `sys_ipc_invoke_raw` entry point.

The implementation does not close the full production IPC gate yet. Remaining work includes:

- explicit kernel reply objects in the object table and capability namespace;
- capability transfer of multiple capabilities per message;
- out-of-line payload transport;
- a scalable timeout queue rather than bounded thread scanning;
- formal cancellation race tests;
- chained scheduling-context donation and budget donation;
- QEMU SMP runtime certification and fault-injection evidence.
