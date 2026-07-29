# Memory-object manager

The manager provides serialized physical-page allocation plus bounded frame and page-table objects. Frame map/unmap operations are serialized by the mapping lock, update the architecture page tables transactionally, and commit or remove reverse-mapping records only after the architecture operation succeeds.

Reverse mappings use generation-checked address-space object references instead of reusable numeric space IDs. A frame may be mapped at up to eight virtual addresses. Frame destruction remains busy until every mapping is removed. Address-space teardown scans the bounded reverse map and removes all records belonging to the exact address-space generation.

Frame, page-table, and resource destruction retire capability authority before
object-table unregister. Frame allocation state is retired under the mapping
lock before physical memory is returned, and map revalidates that state after
acquiring the same lock. The authority lock is deliberately released before
the object-table reader grace period so a remote syscall waiting for authority
cannot deadlock unregister.

Certification validates reverse-map counts, live address-space generations,
mapping generations, permissions, attributes, capability authorities, and
virtual-address uniqueness both before boot handoff and after the complete SMP
workload.

This remains an intermediate mechanism. Scalable indexing, capability-derived mapping authority, device/cache attributes, and controlled revoke-versus-map race evidence are still required.
