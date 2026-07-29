# ARM64 ASID lifecycle

ASIDs are allocated from a bounded generation-tagged bitmap. Exhaustion
advances the generation and performs global stage-1 invalidation before reuse.
Live address spaces refresh stale handles lazily; release ignores handles from
older generations so they cannot free current ownership.
