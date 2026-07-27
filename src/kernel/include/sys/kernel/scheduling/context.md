# Scheduling context

A scheduling context carries base/effective priority, budget, period,
consumption, donated ticks, next replenishment, affinity, throttle state, and
bounded donation depth. Runnable selection is priority ordered with
deterministic rotating tie order. Exhausted contexts are ineligible until
periodic replenishment unless they are executing on donated budget.

Synchronous IPC moves the caller's remaining budget and inherited priority to
the server. A server that makes a nested call propagates both values, with a
maximum chain depth of eight. Reply, timeout, cancellation, server exit, and
teardown return unused ticks to the caller and restore the server's base
priority. Consumed donated ticks remain charged to the original chain.

Per-CPU timeout queues are ordered by absolute deadline. Arming the same thread
replaces its previous entry. Timer expiry pops only due entries, validates the
thread generation and current deadline, and uses a non-blocking lifecycle claim
so IRQ context never spins behind an interrupted IPC transition.

The mechanism is compiled into product builds. Measured latency limits and
multi-hour real-time stress evidence remain required before production
certification.
