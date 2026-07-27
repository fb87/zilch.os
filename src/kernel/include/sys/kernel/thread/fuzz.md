---
module: sys.kernel.thread.fuzz
layer: kernel
status: bring-up
sources:
  - scheduler.hh
  - thread.hh
  - ../../../../../../../user/bootstrap/embedded_images.S
---

# Deterministic user fuzz harness

## Purpose

Exercise the native `sys_ipc` entry, capability validation, operation validation,
thread identity checking, user fault containment, timer preemption, and context
switching from isolated user address spaces.

## Reproducibility

Each thread receives a fixed 64-bit seed derived from its thread identifier.
Failure reports contain the seed, iteration, thread identifier, case, actual
result, and expected result.

## Roles

- Thread 0: valid calls.
- Thread 1: invalid capability selectors.
- Thread 2: invalid IPC operations.
- Thread 3: mismatched thread identity.
- Thread 4: deliberate unmapped data access after 64 iterations.
- Thread 5: valid randomized payloads.
- Thread 6: boundary capability selectors.
- Threads 7-9: deterministic mixed workloads.

## Containment

A user instruction or data abort marks only the current thread faulted and
selects another runnable user thread. Kernel and hypervisor faults remain fatal.

## Scope

This milestone remains pinned to CPU0. Cross-CPU endpoint synchronization and
multi-CPU user scheduling are intentionally deferred.
