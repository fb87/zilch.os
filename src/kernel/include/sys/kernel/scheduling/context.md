# Scheduling context

A scheduling context carries base/effective priority, budget, period, consumption, next replenishment, affinity, throttle state, and bounded priority-donation depth. Runnable selection is priority ordered with deterministic rotating tie order. Exhausted contexts are ineligible until periodic replenishment.

The mechanism is compiled into product builds. Latency limits, timeout queues, donation through IPC, and real-time stress evidence remain required before production certification.
