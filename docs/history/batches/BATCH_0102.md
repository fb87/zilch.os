# Batch 0102 — Generation-safe mapping database foundation

This batch hardens the bounded stage-1 mapping database without claiming the final scalable memory subsystem.

## Delivered

- serialized frame mapping transactions;
- generation-safe address-space references in reverse mappings;
- up to eight mappings per frame;
- exact-address partial unmap;
- frame-wide and address-space-wide cleanup;
- strict permission-bit and W^X validation;
- PL3 multi-map/destroy lifecycle certification.

## Deferred

- scalable mapping trees or hashes;
- capability-derived mapping authority;
- device-memory/cacheability attributes;
- revoke-versus-map and TLB-shootdown race fuzz;
- full memory exhaustion and fragmentation policy.
