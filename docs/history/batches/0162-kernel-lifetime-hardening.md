# Batch 0162: capability and IPC lifetime hardening

This batch begins the chapters 2–6 kernel-hardening sequence with two concrete
ABA fixes.

Capability derivation records now expose generation-tagged handles instead of
reusable raw array indexes. A deleted intermediate record remains reserved
while an active child references its exact generation, descendant traversal
can cross that inactive record, and a generation that would wrap retires the
record. Certification deletes an ancestor, retains a grandchild, forces the
allocator hint back to the deleted index, verifies that reuse is rejected,
and then revokes the complete descendant chain.

IPC cancellation now locks the resolved endpoint before the global lifecycle
transaction. It revalidates the blocked state and waited-on selector while
both locks are held, removes endpoint membership, clears matching reply and
donation state, and publishes the cancellation completion before releasing
the locks in reverse order.

The deterministic derivation test, existing cross-CPU revoke/transfer,
object-destroy/reuse, IPC lifecycle race suites, root SMP fuzz, and complete
four-CPU certification acceptance pass. CAP-016 is complete. IPC-005 and
IPC-006 advance but retain controlled instruction-level interleaving work.
