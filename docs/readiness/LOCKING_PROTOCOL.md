# Kernel locking protocol

All blocking kernel spinlocks follow one global outer-to-inner rank order:

1. endpoint;
2. IPC lifecycle;
3. scheduler timeout queue;
4. capability authority;
5. capability registry or memory mapping;
6. CSpace, ordered by increasing lock address when two are held;
7. capability derivation;
8. physical-memory allocator;
9. ASID/VMID translation-identifier allocator;
10. object table.

Locks must be released in exact reverse order. Equal-rank locks may only nest in
increasing address order, which makes two-CSpace and registry-wide scans
deterministic. Recursive acquisition is forbidden.

The endpoint-before-IPC-lifecycle rule protects queue and blocked-thread state.
Per-CPU timeout queues disable local IRQs while holding their short bounded lock;
they never acquire IPC lifecycle while held.
Capability mutations start at the authority lock; registry-wide operations then
lock registered CSpaces in increasing order. Mapping teardown may run under
capability authority. The allocator and object table are terminal lifecycle
locks and must not call back into lower-ranked subsystems while held.

Certification builds maintain a 16-entry held-lock stack per CPU. Every
instrumented acquisition checks rank, equal-rank address order, recursion, and
maximum depth. Every release checks strict LIFO identity. Violations are retained
in a monotonic counter and fail certification. Release builds compile the
checker state and calls away while preserving the same acquisition structure.

The bounded `printk` lock is deliberately outside this hierarchy: it disables
local interrupts, never nests into kernel lifecycle locks, and falls back to the
lock-free emergency ring on contention.
