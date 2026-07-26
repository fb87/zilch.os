# Kernel Profile 1.0 acceptance

## Kernel contract

- IPC syscall is capability-authorized and synchronous.
- Control syscall exposes capability lifecycle, thread control, mapping,
  notification, interrupt binding, and scheduling-context configuration.
- Every object lookup validates selector, object type, rights, and generation.
- User faults are delivered by IPC to a configured pager.
- Writable-executable user mappings are rejected.
- Cross-CPU wakeup and TLB invalidation use explicit IPIs.

## Runtime gates

1. Four ARM64 CPUs online with timer and reschedule progress.
2. All per-CPU fuzz counters advance with zero unexpected failures.
3. Expected user fault is delivered to the pager and terminated or resumed.
4. Capability copy/move/delete/revoke reject stale or excessive authority.
5. Map/unmap rejects W+X and invalid selectors.
6. Notification and IRQ binding use capabilities only.
7. Endpoint teardown leaves no blocked stale thread references.
8. No EL1 or EL2 exceptions during the soak run.

## Compatibility transition

The embedded deterministic SMP image remains the acceptance workload. The
separately linked `init.elf` is the Profile 1.0 root-server image and uses the
same frozen control ABI. Loading it as the sole initial task requires the
planned earlyfs ELF-loader switch, but no further kernel object-model change.
