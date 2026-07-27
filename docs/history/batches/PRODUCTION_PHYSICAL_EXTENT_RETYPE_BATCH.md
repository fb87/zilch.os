# Batch 0111 — Physical extent ownership and bounded retyping

This batch replaces quota-only resource delegation with explicit page-aligned physical extents. Parent resources carve ranges into child resources, resource-backed frame and page-table creation may allocate only from those ranges, and empty child resources merge their extents back into the parent.

The implementation is intentionally bounded to 16 extents per resource and fixed frame/page-table metadata pools. It is a production-development foundation, not completion of the memory gate.
