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

## Acquisition discipline

Kernel spinlocks are ticket locks, not test-and-set. Fairness is a correctness
requirement here rather than a tuning choice: this kernel has no blocking wait
-- `process_wait` and `notification_poll` return `busy` and require the caller
to poll again -- and every such poll resolves a capability, which takes the
global authority lock. Under an unfair exchange the polling CPU re-acquires a
line it already holds exclusively and wins essentially every race, starving the
thread it is waiting for. FIFO order puts each new attempt behind the existing
waiters and bounds every other CPU's wait.

Two counters are packed into the single word each lock already occupies:
index 0 is the ticket being served, index 1 the next to hand out. Equal halves
mean unlocked, so a zero-initialized word is an unlocked one.

**A lock word must never be reset, zeroed, or otherwise rewound.** Under
test-and-set, storing zero was a harmless force-unlock; under a ticket lock it
rewinds the queue, and a CPU already holding ticket N then waits for a counter
that will never reach it again. Object `initialize` routines therefore
deliberately do not touch their lock word — a released lock is already
unlocked, and these objects live in statically zero-initialized storage, so a
first-time initialize finds an unlocked word anyway.

Because a ticket holder must eventually run to release its turn, nothing may
take a ticket and then fail to make progress. This holds today because the IRQ
path never switches threads — it reprograms the timer and returns — so a thread
spinning for a ticket keeps its CPU. Any future change that makes the kernel
preemptible while a lock is held, or that can terminate a thread mid-spin, has
to revisit this.

Certification builds maintain a 16-entry held-lock stack per CPU. Every
instrumented acquisition checks rank, equal-rank address order, recursion, and
maximum depth. Every release checks strict LIFO identity. Violations are retained
in a monotonic counter and fail certification. Release builds compile the
checker state and calls away while preserving the same acquisition structure.

The bounded `printk` lock is deliberately outside this hierarchy: it disables
local interrupts, never nests into kernel lifecycle locks, and falls back to the
lock-free emergency ring on contention.
