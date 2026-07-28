# Batch 0158: RT-safe deferred logging and release hygiene

Add `printk::defer`, a bounded structured logging API for RT and exception
paths. It writes directly to the lock-free per-CPU emergency ring without
taking the console lock, disabling interrupts, allocating, or consulting the
scheduler. SCH-018 is closed; formatting and asynchronous draining remain the
separate OBS-003 target.

The build-boundary checker now ignores generated `out/` and `.git` trees, so
release source exports can contain their own top-level build files without
polluting the kernel repository ownership check.
