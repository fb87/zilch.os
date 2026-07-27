# Translation identifier lifecycle

ARM64 ASIDs and VMIDs are allocated as `(identifier, generation)` pairs. Zero is
reserved in both namespaces.

Each allocator maintains a bounded bitmap, current generation, allocation count,
and rollover count under a dedicated spinlock. Crossing the bounded allocation
threshold performs a global architectural invalidation, advances the nonzero
generation, and resets the bitmap. A live address space or VM from an older
generation is stale but safe: before installing TTBR0, changing stage-2
mappings, resetting a VM, or entering a guest, it lazily acquires an identifier
in the current generation.

Release invalidates translations before returning an identifier. A stale
generation release never clears a bit in the current namespace, preventing an
old object from freeing a newly assigned identifier.

Certification forces both namespaces through rollover while bootstrap objects
remain live, confirms generation advancement and global invalidation, refreshes
the live objects, and then completes real PL3 scheduling and real guest
execution. Subsequent object destroy/reuse and VM lifecycle suites prove the
refreshed identifiers remain usable.
