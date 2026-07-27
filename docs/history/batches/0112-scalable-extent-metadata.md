# Batch 0112 — Scalable extent metadata and fragmentation recovery

## Scope

This batch replaces the fixed sixteen-entry extent array embedded in each
memory-resource object with a shared bounded metadata pool. It improves
fragmentation handling without claiming an unbounded production allocator.

## Implementation

- 256 reusable extent nodes shared by all memory resources.
- Per-resource linked extent lists sorted by physical address.
- Deterministic high-address delegation from parent resources.
- Node transfer for whole-extent delegation and return.
- Split-node allocation only when delegation divides an extent.
- Adjacent extent coalescing during every return operation.
- Rollback returns all partially carved nodes to the parent on metadata
  exhaustion.

## Certification

`memory_extent_metadata` delegates twenty one-page child resources, destroys
children in alternating order to create fragmentation, then redelegates the
entire twenty-page range and allocates a frame from it. Successful cleanup
proves deterministic merge and metadata-node reuse.

## Limitations

- The global metadata pool remains bounded at 256 nodes.
- No concurrent split/retype/reclaim stress is included yet.
- No full-RAM near-exhaustion or long-duration fragmentation soak is included.
- Allocation policy remains first-fit inside resource-owned extents.
