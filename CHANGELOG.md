
## 0078 - ARM64 ELF instruction-cache coherency

- Synchronize data and instruction caches after loading a user ELF image.
- Prevent stale instructions when process/address-space slots are destroyed and reused.
- Document the SMP slot-reuse regression and required runtime evidence.

## 0079 - Product/test separation

- Removed certification operations from the production `sys` ABI enums.
- Added a separate certification-only test ABI and userspace wrapper.
- Excluded the ARM64 guest verification payload from release compilation.
- Renamed model-only hypervisor records to make modeled execution explicit.
- Strengthened the production ELF gate against test payloads and markers.
