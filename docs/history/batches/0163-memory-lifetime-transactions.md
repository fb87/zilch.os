# Batch 0163: memory lifetime transactions

This batch extends chapters 2–5 kernel hardening across capability authority,
physical memory, reverse mappings, object reclamation, and SMP acceptance.

Frame, page-table, and memory-resource destroy paths now hold capability
authority while validating the object, retiring its resources, and revoking
all references. They then release authority before unregister waits for remote
object readers. This two-phase order excludes new authorized operations
without deadlocking a remote syscall that entered a read-side section before
waiting for the authority lock.

Frame release publishes `allocated=false` under the global mapping lock before
returning the physical page. Mapping revalidates allocation and mapping
attributes after taking that same lock. Explicit frame allocate/release
control operations also hold capability authority, serializing them with map,
unmap, revoke, and destroy.

The mapping database now has a complete bounded invariant scan covering:

- recorded count versus live records;
- allocated-frame ownership of every record;
- address-space object generation and type;
- nonzero mapping generation;
- permission and memory-attribute validity;
- live frame and address-space capability derivations;
- uniqueness of each address-space virtual address.

Bootstrap and final certification both require the scan to pass. Final
acceptance additionally requires object accounting and lock-order invariants.
The complete four-CPU root workload passes with zero acceptance failures.
