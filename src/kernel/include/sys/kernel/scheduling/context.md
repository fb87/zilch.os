# Scheduling context

A scheduling context carries base/effective priority, budget, period,
consumption, donated ticks, a bounded ordered replenishment queue, affinity,
throttle state, and bounded donation depth. Each charged budget slice is
replenished exactly one period after it was consumed; the queue is bounded and
sorted by absolute deadline. Runnable selection is priority ordered with
deterministic rotating tie order. Exhausted contexts are ineligible until due
slices return unless they are executing on donated budget.

Synchronous IPC moves the caller's remaining budget and inherited priority to
the server. A server that makes a nested call propagates both values, with a
maximum chain depth of eight. Reply, timeout, cancellation, server exit, and
teardown return unused ticks to the caller and restore the server's base
priority. Consumed donated ticks remain charged to the original chain.

Per-CPU timeout queues are ordered by absolute deadline. Arming the same thread
replaces its previous entry. Timer expiry pops only due entries, validates the
thread generation and current deadline, and uses a non-blocking lifecycle claim
so IRQ context never spins behind an interrupted IPC transition. Idle CPUs
program the queue head directly; runnable CPUs retain a one-tick scheduling
quantum. Long one-shot intervals advance logical time by their programmed delta.

The mechanism is compiled into product builds. Lock-order instrumentation
reports the maximum observed hold duration in architectural timer ticks.
Measured IRQ-disabled latency limits and multi-hour real-time stress evidence
remain required before scheduler production certification.

Public reconfiguration is permitted only after the target thread has been
explicitly suspended and reached execution quiescence. The target must hold no
reply authority or active donation. Priority is range checked before narrowing,
budget and period pass the production `configure()` validator, and affinity is
retained from the pinned CPU. Runnable or blocked targets return `busy`; this
prevents a remote timer or scheduler pass from observing partially reset
budget, replenishment, priority, or donation state.
