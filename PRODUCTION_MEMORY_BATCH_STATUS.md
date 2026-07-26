# Production memory batch status

## Implemented

- QEMU RAM size is a single top-level Make contract (`MEMORY_MB`) used by both
  the kernel build and `make run`.
- Kernel-image end is exported by the linker and excluded from allocation.
- Remaining QEMU RAM is managed by a page bitmap.
- Physical pages are zeroed on allocation and before release.
- Frame capabilities receive dynamically allocated physical backing.
- Frame backing has explicit ownership and release semantics.
- A frame may have multiple reverse mappings, bounded with explicit exhaustion.
- Unmap identifies the address-space and optional virtual address.
- Page-table capability objects receive allocator-backed physical pages.
- Certification verifies release/reallocation and zero-on-reuse.

## Verification pending

- Top-level `make BUILD_VARIANT=certification run` on QEMU.
- RAM-size matrix: 128, 256, 512, 1024 MiB.
- Allocation exhaustion and pressure testing.
- Concurrent allocate/release/map/unmap race testing.

## Still open

- Dynamic creation of frame/page-table capability objects rather than a bounded
  handle pool.
- Hierarchical page-table construction across the full user VA range.
- Userspace memory-server protocol and authority delegation.
- Pager-driven allocation and mapping reply protocol.
- Scalable reverse-mapping storage.
- Page pinning and DMA semantics (not required for QEMU-only 1.0 unless a
  virtual DMA model is added).
