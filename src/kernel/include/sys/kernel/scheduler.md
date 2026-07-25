---
module: sys.kernel.scheduler
layer: kernel
status: bringup
sources:
  - scheduler.hh
tests:
  - scheduler.tt
  - scheduler_test.tt
---

# Scheduler

## Purpose

Provide fixed-capacity per-CPU run queues, an idle execution object for every
online CPU, timer-driven quantum expiration, and SGI-driven rescheduling.

## Current milestone

The current execution objects are stackless kernel worker state machines. A
worker performs one bounded step whenever selected. This validates queue
rotation, CPU pinning, timer preemption policy, and reschedule IPI integration
without pretending that saved kernel stacks and full register context switching
already exist.

## Invariants

- A run queue is owned by one logical CPU.
- A thread is inserted into at most one run queue.
- Run queues and worker storage are statically bounded.
- Interrupt handlers never allocate memory.
- Timer and reschedule IRQ paths perform bounded work.

## Next milestone

Extend the architectural exception frame with ELR, SPSR, and saved stack state,
then replace worker-step dispatch with real kernel-thread context switching.
