
## 0087 — Quiescent teardown and `.mk` normalization

- Added an explicit remote-CPU quiescence handshake before thread/process teardown and address-space image reuse.
- A thread now publishes whether a CPU is actively executing its PL3 context; suspend and destroy wait for the exception/scheduler path to acknowledge departure from userspace.
- Replaced every subdirectory `Makefile` object-list fragment with `build.mk`; the repository root is the only project entry point named `Makefile`.
- Updated the low-level non-recursive build engine and boundary checks for the `.mk` convention.

## 0084 - Hypervisor header dependency fix

- Made `sys/kernel/hypervisor/object.hh` self-contained by directly including `sys/kernel/object/table.hh`.
- Fixes missing `object::reference_t`, `object::table_capacity`, and `object::resolve()` declarations after the 0082/0083 module split.
- Verified ARM64 certification/release and AMD64 compile-only builds.


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

## 0081 - SMP instruction-cache coherency on ELF slot reuse

- Replace local `ic ivau` invalidation in the ARM64 bootstrap ELF loader with
  `ic ialluis` after cleaning the replacement image to the point of unification.
- This makes reused virtual image addresses coherent across the inner-shareable
  CPU domain and prevents secondary CPUs from executing stale instructions from
  a previously loaded ELF.
- Keep this conservative whole-I-cache operation limited to the bounded
  bootstrap loader; the future runtime process loader should use targeted
  cross-CPU synchronization tied to address-space residency.

## 0.8.3 - Documentation and ownership split

- Refactored top-level project documentation into the canonical `docs/` hierarchy.
- Moved ARM64 root bootstrap image packaging to `src/user/bootstrap/`.
- Moved the certification guest blob adapter and scheduler/acceptance harnesses to `tests/`.
- Added production no-op verification hooks with certification-time overrides.
- Added documentation-layout and user/kernel ownership release gates.

## 0088 - User execution-state publication fix

- Publish PL3 execution state on every lower-PL synchronous exception entry and return.
- Mark the initial PL3 entry as executing.
- Preserve bounded remote-thread quiescence without treating blocked syscall/IPC threads as permanently active.
- Fix the certification cascade where pager teardown returned busy and later lifecycle tests failed.

## 0089 - Transition-safe user-thread quiescence

- Removed generic EL0 exception-entry/return publication of thread quiescence.
- A thread now remains non-quiescent while a syscall or fault handler may return to it.
- Quiescence is published only at explicit hand-off points such as blocking or scheduler deschedule.
- Preserves remote-CPU teardown safety without allowing address-space reclamation during active kernel handling.
