# Memory-object manager

The manager provides serialized physical-page allocation plus bounded frame and page-table objects. Frame map/unmap operations are serialized by the mapping lock, update the architecture page tables transactionally, and commit or remove reverse-mapping records only after the architecture operation succeeds.

Reverse mappings use generation-checked address-space object references instead of reusable numeric space IDs. A frame may be mapped at up to eight virtual addresses. Frame destruction remains busy until every mapping is removed. Address-space teardown scans the bounded reverse map and removes all records belonging to the exact address-space generation.

This remains an intermediate mechanism. Scalable indexing, capability-derived mapping authority, device/cache attributes, and controlled revoke-versus-map race evidence are still required.
