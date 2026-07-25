---
module: sys.kernel.scheduler_test
layer: kernel
status: bringup
sources:
  - scheduler_test.hh
tests:
  - scheduler_test.tt
---

# Scheduler timer demonstration

Creates ten CPU-pinned stackless kernel worker threads distributed round-robin
across the online CPUs. Worker delays range from one through ten scheduler
executions. Each worker prints its thread ID, actual CPU ID, execution counter,
and configured delay continuously for the lifetime of the kernel.

Bring-up verification requires all ten workers to produce at least three reports. After verification, every worker continues running and reporting forever. This proves
per-CPU run-queue rotation, timer-driven scheduler execution, concurrent CPU
progress, and SMP-safe logging. It is still a policy demonstration rather than
saved-stack context switching.
