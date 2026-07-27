## 0128 - Unmapped kernel stack guard pages

- Replace the coarse ARM64 kernel identity block with a shared L2 identity map.
- Split the 2 MiB stack window into L3 pages shared by kernel and user TTBR0 roots.
- Place each EL1 and EL2 stack in a 64 KiB slot with an unmapped page immediately below its 32 KiB usable region.
- Validate every guard and adjacent usable page during kernel bootstrap certification.

## 0127 - Kernel stack bounds and canaries

- Seed independent canary regions at every EL1 and EL2 per-CPU stack base.
- Validate stack bounds and canaries on every ARM64 exception dispatch.
- Retain the lowest observed stack pointer for per-CPU EL1/EL2 high-water analysis.
- Halt with a dedicated diagnostic before continuing on detected stack corruption.

## 0126 - Object lookup/use quiescence

- Add per-CPU object-table read-side sections around ARM64 exception dispatch.
- Remove an object from the generation-checked table before waiting for pre-existing remote readers.
- Delay backing-object reuse until every remote lookup/use section has completed.
- Add a cross-CPU notification lookup/signal versus destroy/reuse certification race.
- Increase disjoint EL1 and EL2 per-CPU stacks from 16 KiB to 32 KiB after the wider dispatch path exposed stack-frame corruption under race teardown.

## 0125 - Authority-safe capability mutation API

- Make public install, derive, copy, mint, move, remove, and delete operations acquire the authority lock.
- Add explicit `_locked` mutation primitives for callers already inside an authority transaction.
- Route control-path and IPC transactions through those locked primitives to avoid recursive locking.

## 0124 - Atomic capability lookup snapshots

- Lock CSpaces while reading capability slots.
- Validate type, derivation activity, rights, and object generation against one slot snapshot.
- Prevent concurrent delete/revoke from exposing mixed capability fields to lookup callers.

## 0123 - Capability transfer/revoke serialization

- Serialize IPC capability minting with control-path copy, mint, delete, revoke, and mapping authority.
- Make public descendant and object-reference revocation acquire the authority lock by construction.
- Add a cross-CPU transfer-versus-revoke certification race with a linearization-independent postcondition.
- Route the race roles to the pager-client image and grant endpoint receive authority only to the server.
- Make the bootstrap pager fixture use the pager-client image instead of assuming the init image leaves page four unused.
- Move root mapping-database scratch mappings outside the expanded init image.

## 0122 - IPC lifecycle serialization

- Serialize reply, cancellation, timeout, exit, and teardown ownership changes.
- Remove blocked senders with the endpoint right appropriate to their wait state.
- Restore error-only IPC completions instead of returning stale syscall registers.
- Make timer expiry non-blocking in IRQ context and retry lifecycle contention.
- Cancel live reply authority when a server exits.
- Add four-CPU cancellation, timeout, blocked-destroy, server-exit, and endpoint-reuse evidence.

## 0120 - Disjoint memory-service selectors

- Reserve root CSpace selectors 40-51 for the memory server and three clients.
- Remove aliasing between client 2 and the server task/address-space selectors.
- Add compile-time ordering and range checks for the service graph.
- Preserve the production ABI and memory-server protocol.

## 0119 - Userspace thread exit lifecycle

- Add a production `thread_exit` control operation.
- Make the userspace runtime terminate and deschedule the current thread when `main()` returns.
- Prevent completion-notification versus PL3-exit races during process teardown.
- Correct certification client-ID decoding so origin bits are not reported as client bits.

## 0111 — Physical extent ownership and bounded retyping

- Add explicit physical extents to memory-resource objects.
- Carve page-aligned extents from parent resources during delegation.
- Allocate resource-backed frames and page tables only from owned extents.
- Merge empty child extents back into the parent on destruction.
- Prevent unrestricted allocation from consuming active child extents.
- Add nested split/retype/exhaust/reclaim certification coverage.

## 0109 — Userspace memory-resource delegation

- Add bounded parent/child memory-resource capability objects.
- Give root an explicit resource capability and each PL3 task a delegated quota.
- Route the userspace memory server through resource-backed frame creation.
- Add quota exhaustion, accounting, release, and resource destruction certification.

## 0.8.0 — Batch 0107

- Preserve the ARM64 firmware DTB pointer through early assembly startup.
- Parse DTB memory nodes, reserve-map entries, `/reserved-memory`, and the DTB blob.
- Support bounded discontiguous physical allocator regions.
- Export memory inventory through bootinfo v2.
- Add bootstrap validation for region ordering, accounting, and root metadata.

## 0106 - Reserve bootstrap certification mapping window

- Move kernel bootstrap scratch mappings away from the growing `init.elf` load segment.
- Reserve `user_code + 0x8000` and `user_code + 0x9000` for bootstrap-only mapping checks.
- Add a compile-time guard keeping the reserved test window below the user stack.
- Fix certification boot failure `kernel bootstrap self-test failed=-5` caused by a legitimate page collision.
- No production ABI or runtime memory policy changes.

## 0100 - Dynamic physical-memory lifecycle foundation


## 0101 - Certification memory-test naming fix

- map certification test ID 13 to `memory_resource_lifecycle`;
- prevent successful memory lifecycle evidence from being reported as `unknown`;
- no kernel mechanism, ABI, scheduler, or memory-allocation semantics changed.

- Serialize physical-page allocation and release across CPUs.
- Publish the allocatable QEMU RAM range as an explicit physical-region inventory.
- Tag dynamic frame and page-table objects with owner identity.
- Remove tracked frame mappings during address-space teardown.
- Add PL3 certification for balanced frame/page-table allocation, accounting, destruction, and reuse.
- Reconcile MEM and TST readiness evidence without claiming full untyped-memory delegation.


## 0095 - Generation-tagged CPU return-frame ownership

- Track the generation of the thread bound to each CPU, not only the reusable slot index.
- Prevent stale CPU bindings from mutating or clearing a newly created thread generation.
- Require teardown to observe both `executing=false` and no CPU binding to the target generation before reclaiming the bundle.
- Preserve conservative ASID and instruction-cache synchronization for the bounded bootstrap loader.
## 0093 - Atomic scheduler execution claims

- Close the scheduler-versus-teardown resurrection race with conditional state transitions.
- Publish the execution claim before `ready -> running`, and withdraw it when the claim fails.
- Make fault wakeup, deschedule, first entry, and IPC rollback teardown-safe.
- Preserve the remote-CPU quiescence protocol while preventing suspended threads from being dispatched.


## 0091 - ASID reuse barrier and readiness reconciliation

- Invalidate reused ARM64 ASIDs before installing replacement TTBR0 roots.
- Retain the per-CPU instruction-cache invalidation before PL3 return.
- Reconcile CAP-008..013, CAP-016..019, and IPC timeout/wakeup progress in the authoritative readiness checklist.
- Expand the requirement evidence matrix without promoting incomplete production gates.

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

## 0090 - Capability and IPC lifecycle hardening

- Reuse bounded capability derivation records safely.
- Validate active derivations during capability lookup.
- Make capability revoke descendant-based and authority-checked.
- Pass mint badges through the explicit control ABI argument.
- Fix duplicate capability transfer on direct IPC rendezvous.
- Add repeated derivation/revoke/reuse and rights-attenuation certification coverage.

## 0094 — Return-frame ownership quiescence

- Keep a thread's execution claim while a lower-PL exception frame may still be
  returned to it.
- Publish quiescence only after the scheduler commits that frame to another
  thread or to the kernel-idle context.
- Close timer-preemption versus destroy/reuse race that could return a stale
  user frame after teardown.

## 0096 — Transactional user-thread publication

- Dynamic and bootstrap user threads remain `inactive` while their object graph,
  CSpace, address space, initial context, and certification counters are built.
- `ready` is now the final release-store commit point of process creation.
- This prevents a secondary CPU from dispatching a half-created thread during
  immediate destroy/reuse tests.


## 0097 — Permanent kernel-root idle transition

- Switch to the global kernel TTBR0 root before publishing a user thread as quiescent.
- Prevent user address-space teardown from removing the page tables backing an EL1 idle CPU's kernel instruction stream.
- Retain the execution claim across IPC/fault blocking until another user address space or the permanent kernel root is installed.

## 0098 - Acceptance failure ledger

- Preserve a per-test failure count and bit mask in the PL3 certification runner.
- Report the exact failure mask and certification-transport status in the final acceptance line.
- Avoid collapsing every nonzero suite outcome into an uninformative `failures=1` record.

## 0099 - Atomic two-phase capability revoke

- Discover all derivation descendants before invalidating any parent record.
- Prevent recursive revoke from losing grandchildren when an earlier slot is removed.
- Preserve the selected ancestor capability while removing every marked descendant.

## 0102 - Generation-safe mapping database foundation

- Serialize frame map/unmap transactions across CPUs.
- Replace numeric address-space IDs in reverse mappings with generation-checked object references.
- Support up to eight mappings per frame with per-mapping generation records.
- Reject invalid permission encodings and require readable, non-W+X mappings.
- Preserve destroy rejection until every frame mapping is removed.
- Resolve address spaces through the object table during frame-wide teardown.
- Add PL3 certification for two mappings of one frame and balanced unmap/destroy.

## 0103 - Pager permission ABI correction

- Define public ABI memory permission flags instead of using numeric literals.
- Make the userspace memory server request read/write mappings for resolved faults.
- Preserve rejection of write-only and writable-executable stage-1 mappings.
- Prevent pager failure from cascading into later memory and process lifecycle tests.


## 0104 - Capability-bound mapping authority

- Record the exact frame and address-space capability derivations that authorize every mapping.
- Serialize capability mutation against mapping creation and removal.
- Remove mappings automatically when their authorizing capability is deleted or recursively revoked.
- Preserve descendant-only revoke semantics while keeping the selected ancestor capability valid.
- Add PL3 certification for revoke-driven unmapping and subsequent frame destruction.

## 0105 - Memory attributes and pressure rollback

- Add public normal/device memory type and shareability mapping attributes.
- Add root-only platform-allowlisted device-frame creation.
- Emit ARM64 normal or device page descriptors according to validated attributes.
- Reject executable device mappings and normal/device attribute mismatches.
- Add bounded certification for MMIO lifecycle and failed-map accounting rollback.

## 0108 - Robust DTB inventory source selection

- Try the firmware-provided DTB pointer first.
- Probe the QEMU ARM64 virt conventional DTB location at RAM base when the
  firmware register is absent or invalid.
- Fall back explicitly to the platform RAM description instead of halting with
  `not_found`.
- Report the selected inventory source in the memory boot log.
- Keep the fallback freestanding without implicit `memcpy` dependencies.

## 0110 - Bootstrap object ID allocation map

- Fix the root memory-resource object ID collision with the root notification.
- Centralize every fixed bootstrap object ID in `object::bootstrap_id`.
- Reserve non-overlapping ranges for threads, tasks, endpoints, frames, page
  tables, notifications, interrupts, scheduling contexts, address spaces,
  hypervisor objects, and the root memory resource.
- Add compile-time range and uniqueness checks below the dynamic object ID base.

## 0112 - Scalable extent metadata and fragmentation recovery

- Replace per-resource fixed extent arrays with a shared 256-node extent metadata pool.
- Keep each resource's extents in deterministic physical-address order.
- Split and transfer extent nodes without copying physical ownership records.
- Merge adjacent extents on every return and reuse released metadata nodes.
- Add PL3 certification that creates twenty one-page child resources, returns
  them in alternating order, redelegates the fully merged range, and verifies
  frame allocation and complete cleanup.


## 0113 - Memory pressure and rollback injection

- Add certification-only extent-node allocation fault injection through the verification boundary.
- Add memory invariant snapshots covering free pages, extent nodes, resources, frames, page tables, and resource accounting.
- Verify failed extent splitting rolls back child creation, parent delegation, metadata, and capability state.
- Add 32 quota-exhaustion/reclaim cycles covering 512 resource-backed frame lifecycles.
- Require the complete invariant signature to match before and after the pressure workload.

## 0114 - PL3 memory-server request protocol

- Added a public memory-server request ABI and userspace IPC call wrapper.
- Added a separately linked memory pressure client ELF.
- Added three-CPU PL3 allocation/query/release pressure through the memory server.
- Added owner-bound frame handles and clean shutdown validation.

## 0115 - Reply capability transfer and client-owned frames

- Extend IPC reply and reply-receive with optional single-capability transfer.
- Commit reply transfer before consuming the single-use reply authority, so a
  busy destination leaves the caller blocked and permits a retry or error reply.
- Transfer resource-backed frame capabilities from the PL3 memory server into
  receiver-selected client CSpace slots.
- Roll back the server-side frame and handle when a selected destination slot
  is occupied, then report the transfer error through the preserved reply.
- Require clients to delete received capabilities before releasing server
  handles.
- Exercise successful delivery and occupied-slot rollback across three
  concurrent PL3 clients.


## 0116 - Aggregate memory-client completion badges

- Fix the root certification runner dropping valid notification badges that arrive out of order.
- Accumulate notification bits until the complete expected mask has been observed.
- Wait for all three concurrent memory-client completion badges as one set.
- Preserve immediate failure handling for high-order memory-server failure badges.

## 0116 - Memory-client capability deletion fix

- Pass the current-task selector (`0`) explicitly when deleting frame
  capabilities received from the memory server.
- Prevent received frame slots from being misinterpreted as task selectors,
  which caused `denied` results and aborted all memory clients before release
  and shutdown.

## 0117 - Memory server protocol diagnostics

- Add certification-only stage-coded failure badges for memory clients.
- Preserve and report the exact server/client protocol failure origin.
- Accumulate completion badges without losing concurrent client signals.
- Keep the production ABI and successful protocol behavior unchanged.

## 0118 - Memory protocol lifecycle diagnostics

- Increase the bounded protocol wait budget for capability-transfer pressure.
- Report timeout badge state, per-client teardown failures, and server teardown failures.
- Keep production ABI and runtime semantics unchanged.

## 0121 - Atomic exit notification

- Extended `thread_exit` with optional notification selector and badge arguments.
- Memory pressure clients now publish completion and terminate in one kernel transition.
- Retained teardown diagnostics now include the exact returned error code.
