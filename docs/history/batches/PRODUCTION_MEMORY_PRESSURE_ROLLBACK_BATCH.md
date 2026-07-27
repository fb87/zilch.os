# Production memory pressure and rollback batch 0113

This batch adds certification-only failure injection at extent-node allocation,
a stable memory invariant snapshot, and repeated quota-pressure/reclaim coverage.

The `memory_pressure_rollback` PL3 test forces a split-allocation failure and
verifies transactional cleanup, then performs 32 cycles of sixteen-frame
resource exhaustion and reclamation. The before/after invariant signature must
match exactly.

This is bounded certification evidence. Multi-CPU allocator pressure, full-RAM
exhaustion, and per-stage injection beyond extent metadata remain open.
