# Batch 0147 — Real PL3 undefined-instruction fault containment

- Define public ABI v1 fault-kind and disposition values with kernel
  compile-time equivalence checks.
- Add an ARM64 pager-client workload that executes a real `udf #0`.
- Route the workload through the controlled bootstrap image registry.
- Have the userspace pager validate instruction-fault metadata and apply
  terminate policy through one-shot reply authority.
- Verify the pager and complete userspace service graph remain operational
  after the faulting process is contained.
