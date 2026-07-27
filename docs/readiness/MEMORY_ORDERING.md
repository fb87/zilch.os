# Kernel memory-ordering rules

Kernel synchronization follows these rules:

- Spinlock acquisition uses acquire semantics and release uses release semantics.
  Data protected by a lock is non-atomic unless it is also read lock-free.
- Object-table pointers, capability derivation activity, lifecycle state, and
  queue ownership are published with release operations and consumed with
  acquire operations.
- Relaxed atomics are limited to statistics, sequence allocation, or polling
  hints where the value does not publish other memory.
- Emergency and audit records write payload first and publish their sequence
  last with release semantics. Readers load the sequence with acquire semantics
  before trusting the payload.
- Page-table writes complete with the required DSB before TLBI; TLBI completes
  before ISB or return to the affected execution context.
- Device-register programming uses volatile MMIO plus architecture-required
  DSB/ISB ordering. Volatile alone is never treated as inter-CPU synchronization.
- Cross-CPU lifecycle reuse requires generation validation and the object-table
  read-side grace period; an atomic pointer load alone does not grant ownership.

New lock-free structures must document their publication word, memory orders,
ownership transfer, wrap behavior, and reclamation rule beside the
implementation. Sequential consistency may be used for correctness-first
read-side quiescence but must not be weakened without replacement proof.
