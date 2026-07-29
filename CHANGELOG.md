## 0172 - Complete interrupts, timers, and virtual-platform support

- Restrict userspace interrupt objects to the supported SPI inventory and
  reject private, timer, IPI, duplicate, and out-of-range lines.
- Certify exclusive edge and level delivery, notification acknowledgement,
  explicit deactivation, delegation/revoke, and storm containment.
- Validate the versioned QEMU platform inventory and all four per-CPU timer
  databases at final acceptance.
- Add the aggregate interrupt/timer/platform completion gate and reconcile
  unsupported hardware, watchdog/power, and AMD64 runtime scope.

## 0171 - Complete bounded scheduler and real-time behavior

- Add transactional affinity migration for suspended donation-free threads.
- Validate every live scheduling context and per-CPU timeout queue at final
  acceptance.
- Enforce the QEMU profile's IRQ-disabled, IRQ, preemption, cross-CPU wake, and
  IPC service latency bounds.
- Advance six logical hours and 21,600 sporadic-server periods without an
  accounting, throttle, replenishment, or deadline violation.
- Add the aggregate scheduler completion gate and close SCH-001 through
  SCH-024.

## 0170 - Complete the bounded kernel capability system

- Validate every registered CSpace and active derivation at final acceptance,
  including occupancy, generation, ancestry, uniqueness, and live-object
  invariants.
- Add a `capability_completion_gate` aggregating attenuation, derivation,
  revoke, batched transfer and rollback, mapping authority, cross-CPU races,
  SMP mutation, teardown, and generation reuse evidence.
- Freeze the production bounds at 32 CSpaces, 256 slots per CSpace, 4,095
  usable derivations, depth 64, and four-capability IPC batches.
- Close CAP-001 through CAP-021 and the capability production gate.

## 0169 - Complete bounded physical memory and address spaces

- Generate the QEMU virt DTB for every ARM64 run, load it at the documented
  firmware probe address, and reject static platform fallback in final
  certification acceptance.
- Certify discovered reservations by observing two allocatable regions after
  the loaded DTB is excluded from the physical allocator.
- Add a `memory_completion_gate` aggregating pager, frame/page-table resource,
  mapping, revoke, attributes, extent, rollback, SMP, teardown, and reuse
  workloads.
- Close MEM-001 through MEM-026 and the bounded memory production gate.

## 0168 - Close the IPC production gate

- Certify successful two-capability transfer on both call and reply before
  forcing and validating complete partial-batch rollback.
- Exercise frame-grant OOL transport end to end by mapping, writing, reading,
  unmapping, deleting, and releasing the granted page in three clients.
- Add a dedicated `ipc_completion_gate` runtime result aggregating lifecycle,
  transfer, OOL, object-reuse, and dynamic-endpoint certification.
- Reconcile the readiness and evidence documents and close both IPC gate
  checkboxes after the complete certification suite passes.

## 0167 - Complete bounded Core IPC

- Transfer up to four capabilities atomically from a user-described batch,
  rolling back every earlier mint if any entry fails.
- Define bounded out-of-line IPC as a checked one-page frame grant carrying
  offset and length metadata without unbounded kernel allocation or copying.
- Make notification policy explicitly nonblocking and badge-coalescing,
  leaving all blocking, timeout, and donation semantics with endpoints.
- Target normal and teardown IPC wakeups to the owning CPU.
- Gate certification on the IPC latency limit and release builds on static
  call, receive, and reply instruction-footprint budgets.

## 0166 - Target IPC wakeups and validate queue ownership

- Send cross-CPU IPC wakeups only to the receiver's pinned CPU on ARM64
  instead of broadcasting a reschedule SGI to every other CPU.
- Validate that every queued endpoint reference resolves to a live thread,
  appears only once, and cannot simultaneously own the receiver slot.
- Retain cross-CPU wake latency telemetry as end-to-end evidence that the
  targeted CPU received and serviced the wake request.

## 0165 - Retire complete process bundles transactionally

- Quiesce a process thread and destroy its delegated memory resource before
  entering the process-wide capability-authority transaction.
- Revalidate the thread, task, and address-space control capabilities, unmap
  the address space, drain the task CSpace, and revoke the complete object
  bundle before object-reader grace periods.
- Gate final acceptance on thread, task, address-space, and scheduling-context
  ownership and lifecycle invariants.

## 0164 - Retire IPC and interrupt objects safely

- Add an endpoint retiring state and revalidate the control capability while
  holding endpoint-to-authority locks before revocation.
- Reject call and receive operations that resolved an endpoint before its
  retirement became visible.
- Retire notification capabilities under authority and release authority
  before object-reader synchronization.
- Publish IRQ registry entries with atomic compare/exchange, roll back failed
  registration, and consume entries with acquire semantics in dispatch.
- Mask IRQs before rebinding notification state and reject rebinding while an
  interrupt remains active.
- Gate final acceptance on endpoint, notification, and interrupt registry
  invariants in addition to mapping, object, and lock invariants.

## 0163 - Serialize memory-object retirement and mapping lifetime

- Make frame, page-table, and memory-resource destruction capability-authority
  transactions through validation, resource retirement, and capability revoke.
- Release the authority lock before object-table reader synchronization to
  avoid a grace-period cycle with remote syscalls waiting for authority.
- Retire frames under the mapping lock before returning physical memory and
  revalidate allocation state after map acquires that lock.
- Serialize explicit frame allocate/release operations against map, unmap,
  revoke, and destroy.
- Add complete mapping-database validation and make mapping, object-accounting,
  and lock-order invariants part of final certification acceptance.

## 0162 - Harden capability derivations and IPC cancellation

- Replace reusable derivation-array indexes with generation-tagged internal
  handles and permanently retire a record if its generation would wrap.
- Prevent inactive derivation records from being reused while active children
  still reference that exact generation.
- Preserve descendant traversal through deleted intermediate capabilities
  without allowing an unrelated reused record to create an ABA collision.
- Serialize endpoint removal and IPC lifecycle completion under the documented
  endpoint-to-lifecycle lock order, with exact blocked-state and endpoint
  revalidation.
- Add deterministic certification for deleted-ancestor traversal and forced
  derivation-index reuse alongside the existing four-CPU lifecycle races.

## 0161 - Preserve guest virtual timer state and counter offsets

- Save and restore `CNTV_CTL_EL0` and `CNTV_CVAL_EL0` across real guest
  deschedule/resume boundaries.
- Program a per-VM `CNTVOFF_EL2` value for every guest entry and restore the
  host value on normal and rejected entries.
- Track bounded virtual-timer arm, expiry, pending, acknowledgement, and
  cancellation state in the production vCPU path.
- Certify a nonzero counter offset with a real ARM64 guest and exercise
  deterministic expiry and cancellation transitions.

## 0160 - Add kernel real-time latency telemetry

- Wrap serialized logging and scheduler timeout-queue mutation in per-CPU
  architectural-counter duration telemetry.
- Measure IRQ handler service, timer preemption service, cross-CPU wake
  request-to-IPI receipt, and IPC syscall service with the same per-CPU maxima.
- Publish full-run QEMU observations without treating host-scheduling outliers
  as kernel acceptance failures.
- Advance SCH-017 and SCH-020 through SCH-023 to in progress; stable targets
  and retained real-hardware evidence remain required for completion.

## 0159 - Restore progress-safe sporadic replenishment

- Track charged scheduling slices in a bounded ordered replenishment queue.
- Merge equal deadlines and coalesce overflow into the latest record so queue
  pressure delays budget safely instead of permanently throttling a context.
- Certify staggered return, throttling, overflow rejection, bounded-queue
  recovery, and the pager checks that exposed the earlier liveness regression.
- Timestamp real scheduler charges and donated budget from the per-CPU logical
  timer so every exhausted context retains a future progress point.
- Make EL2 hexadecimal diagnostics register-only, preventing diagnostic data
  lookup faults from recursing through the active-guest exception path.
- Close SCH-009 and SCH-010 with complete root-only certification acceptance.

## 0158 - Add RT-safe deferred logging

- Add `printk::defer` for lock-free structured records from RT/exception paths.
- Close SCH-018 while retaining formatted asynchronous draining as OBS-003.

## 0157 - Bound release stack usage

- Emit compiler `.su` records for freestanding C++ objects.
- Enforce an 8 KiB per-function stack bound in ARM64 and AMD64 release gates.
- Close TST-034 while retaining runtime guard-page and canary checks.

## 0156 - Add portable ABI UBSan gate

- Add a Clang UBSan compile/run gate for the portable ABI layout test.
- Close TST-033 while explicitly limiting scope to host-portable ABI code.

## 0155 - Record static-analysis tool readiness

- Add a static-analysis tool inventory gate and deviation report.
- Track missing `scan-build` and `clang-tidy` explicitly as release blockers
  for TST-031 and TST-032 rather than implying unverified cleanliness.

## 0154 - Verify reproducible release artifacts

- Make early filesystem archives deterministic by fixing entry order, epoch,
  and numeric ownership.
- Add a reproducibility gate that performs clean fixed-epoch ARM64 and AMD64
  release builds and compares release artifacts.
- Close TST-036 with passing evidence; linker map paths are normalized only for
  comparison while binaries and archives remain byte-for-byte checked.

## 0153 - Add IRQ timing and binary hardening evidence

- Add release ELF W+X, executable-text, and writable-rodata section auditing.
- Run the binary permission audit for both ARM64 and AMD64 release paths.
- Close TST-035 with explicit limitations documented; SCH-017 remains open.

## 0152 - Enforce sporadic budgets and measure lock holds

- Experimental per-slice replenishment was reverted after it exposed a
  fault-service liveness regression; the stable periodic model remains.
- Reject overflowed scheduling deadlines and retain existing budget validation.
- Record maximum ranked-lock hold duration in timer ticks and report it during
  certification.
- SCH-010 and SCH-017 remain open; SCH-016 lock-hold measurement remains
  certified.

## 0151 - Program per-CPU tickless deadlines

- Drive each ARM64 virtual timer from an absolute local scheduler deadline.
- Integrate idle timer programming with the head of the per-CPU timeout queue.
- Advance logical time by the programmed one-shot delta.
- Validate frequency and interval bounds and saturate deadline arithmetic.
- Close TIM-002, TIM-003, TIM-004, and SCH-019 with live certification evidence.

## 0150 - Bind pager replies to the pending fault

- Reject pager mappings for a page other than the recorded fault page.
- Enforce read, write, and execute permissions against the recorded access type.
- Preserve blocked state and reply authority after invalid replies.
- Certify corrected retry and terminate behavior through real userspace pager policy.

## 0149 - Freeze the fault IPC metadata contract

- Define the four-word ABI v1 fault message as kind, syndrome, address, and PC.
- Deliver architectural syndrome data that was previously retained only inside the kernel.
- Validate complete metadata for real PL3 data and undefined-instruction faults.
- Add structure layout and numeric-value ABI gates.

## 0148 - Make scheduling configuration lifecycle-safe

- Replace unchecked scheduling-field writes with validated context configuration.
- Reject priority truncation and invalid budget/period combinations.
- Require explicit thread suspension and quiescence before scheduling-state reset.
- Certify malformed, live-target, valid configuration, teardown, and reuse paths.

## 0147 - Contain a real PL3 undefined-instruction fault

- Publish ABI v1 fault-kind and disposition values.
- Execute a real ARM64 `udf` instruction from a userspace fault client.
- Validate instruction-fault metadata and apply terminate policy in the userspace pager.
- Keep the pager and remaining service graph operational after containment.

## 0146 - Deliver endpoint capability badges through IPC

- Snapshot the invoking endpoint capability's badge for direct, queued, and fault IPC.
- Return the badge instead of exposing a kernel thread ID to receiving servers.
- Mint generation-tagged endpoint authority for dynamically created tasks.
- Certify rights attenuation and post-accept capability deletion semantics.

## 0145 - Enforce physical IRQ ownership and containment

- Add an exclusive generation-safe IRQ registry and notification binding.
- Route non-reserved GIC interrupts through capability-owned kernel IRQ objects.
- Implement mask, priority drop, explicit deactivate, acknowledge, and unmask sequencing.
- Add level/edge trigger configuration and an explicit no-shared-lines 1.0 policy.
- Track delivery/ack/suppression state and mask interrupt storms after 64 events per window.
- Certify attenuated cross-CSpace delegation, revoke, delivery, acknowledge, and storm containment.

## 0144 - Donate scheduling contexts through IPC

- Propagate remaining budget and inherited priority through synchronous IPC calls.
- Bound nested donation chains at depth eight and return unused budget on unwind.
- Restore server priority and budget ownership on reply, timeout, cancellation, exit, and teardown.
- Replace whole-thread timeout scans with generation-checked per-CPU deadline queues.
- Certify two-hop donation, priority inheritance, chain rejection, budget return, and deadline expiry.

## 0143 - Bound pager failure and duplicate fault resolution

- Serialize fault map/reply state with the IPC lifecycle and mapping locks.
- Treat an already-installed identical page mapping as an idempotent concurrent completion.
- Retain fault deadlines after pager rendezvous and terminate orphaned callers on expiry.
- Consume pending fault authority when a pager exits and reject nested pending-fault overwrite.
- Add duplicate-page, nested-fault, and pager-timeout certification evidence.

## 0142 - Scale and guard capability spaces

- Replace the single 64-slot CSpace array with a two-level 4×64 radix.
- Validate an eight-bit CSpace guard before every capability path resolution.
- Add per-leaf occupancy bitmaps and rotating slot allocation across 256 slots.
- Sort global revoke lock acquisition by CSpace address and release in reverse order.
- Certify cross-leaf allocation, wrong-guard rejection, bulk revoke, and 4,096 cross-CSpace transfer/reuse operations.

## 0141 - Make ASID and VMID rollover generation-safe

- Replace fixed ASIDs with a generation-tagged bounded allocator.
- Add generation-tagged VMID rollover and stale-release protection.
- Globally invalidate stage-1 or stage-2 translations before namespace reuse.
- Lazily refresh stale live address spaces and VMs before execution.
- Force both namespaces through rollover before real PL3 and guest acceptance.

## 0140 - Close remaining self-contained kernel hardening gaps

- Add overflow-safe, page-complete EL0 range validation and unprivileged user-copy primitives.
- Classify ARM64 undefined instructions for the existing fault-IPC path.
- Inventory CSV2, CSV3, SSBS, pointer authentication, and BTI on every CPU and add CSDB/ISB validation barriers.
- Enforce source/header size and dependency boundaries in the production gate.
- Freeze CPU-hotplug, suspend/resume, and AMD64 virtualization as unsupported in 1.0.
- Document the threat model, trust boundaries, memory ordering, PAuth/BTI strategy, and unsupported scope.

## 0139 - Make panic independent of scheduler state

- Centralize fatal exception and stack-corruption handling in a lock-free panic path.
- Mask debug, SError, IRQ, and FIQ exceptions before recording.
- Avoid scheduler, allocator, capability, object-table, and formatted-console dependencies.
- Certify crash capture with poisoned scheduler identity and a deliberately held printk lock.
- Document the retained-record and CPU-park protocol.

## 0138 - Certify the kernel lock hierarchy

- Define one global rank order for every active blocking kernel spinlock.
- Enforce recursive-acquisition, equal-rank ordering, depth, and LIFO-release rules in certification.
- Instrument IPC, capability, memory, and object lifecycle locks.
- Compile checker state out of release images.
- Document the hierarchy and certify zero violations through the full four-CPU workload.

## 0137 - Account kernel and VM resource lifecycles

- Track per-object-type live, peak, created, and destroyed counts at the object table.
- Track per-VM current/peak mapped pages, mapping operations, active vCPUs, and run balance.
- Reject counter saturation and underflow instead of silently wrapping lifecycle state.
- Add release-enabled, sequence-published VM lifecycle audit records.
- Certify dynamic-object and guest map/run/teardown accounting balance.

## 0136 - Freeze native ABI v1

- Freeze native ABI 1.0.0 register, numeric, size, alignment, and field-offset contracts.
- Move fuzz endpoints, cases, and magic values out of the product ABI into certification ABI.
- Expand ABI checks to all public structures, enum widths, trivial-copy/layout properties, and selected numeric values.
- Document compatibility, deprecation, release-class, and diagnostic-version policies.

## 0135 - Lock down product/test boundaries

- Compile IPC fuzz counters, decoders, and progress reporting only in certification builds.
- Deny guest diagnostic hypercalls and omit detailed EL2 page-table/register walks in release.
- Verify release images contain no fuzz, profile-model, acceptance, or HV-walk strings.
- Reconcile the completed VM object, stage-2, virtual IRQ/timer, lifecycle, VMID, and test-module split.

## 0134 - Versioned production trace policy

- Add version-1 IRQ, scheduler, IPC, VM-exit, and user-fault event records.
- Compile routine tracing out of release builds through `CONFIG_TRACE=0`.
- Keep fatal exception and printk-contention capture enabled in every variant.
- Redact guest registers and user/guest address diagnostics from release logging.
- Document stable event identifiers, field layouts, and publication semantics.

## 0133 - Failure-safe emergency diagnostics

- Add lock-free 32-record emergency event rings for each CPU.
- Bound printk lock acquisition and record contention instead of deadlocking in IRQ context.
- Capture every exception entry before scheduler or syscall dispatch.
- Preserve checksummed fatal exception and stack-corruption records in `.noinit`.
- Certify release/acquire publication and readback of CPU-local emergency records.

## 0132 - Enforce kernel page-granular W^X

- Export page-aligned kernel text, rodata, data, and BSS linker boundaries.
- Replace both coarse kernel-image identity blocks with shared L3 page tables.
- Map kernel text RX, embedded user images and rodata RO-NX, and mutable state RW-NX.
- Validate every kernel image PTE at certification bootstrap and reject any writable-executable page.
- Share the protected kernel mapping across the permanent kernel and all user TTBR0 roots.

## 0131 - Contain user and guest faults

- Reconstruct guest fault IPA from HPFAR_EL2 and FAR_EL2 in abort exit records.
- Classify unexpected guest exits as fatal to only the owning vCPU and VM.
- Preserve stage-2 faults as recoverable VMM exits.
- Add negative certification for abort classification, IPA reconstruction, and fatal-exit policy.
- Close user-fault recovery using existing pager IPC and end-to-end acceptance evidence.

## 0130 - ARM privilege and guest-control hardening

- Feature-detect PAN and UAO while retaining the Armv8-A build baseline.
- Enable PAN, disable UAO, and verify both policies during certification bootstrap.
- Sanitize trapped guest SCTLR_EL1 writes to supported control and mandatory RES1 bits.
- Contain unknown guest hypercalls as explicit exits carrying the rejected call number.
- Add negative policy tests for unknown calls and hostile SCTLR values.

## 0129 - Scrub reused memory and architectural state

- Certify that a poisoned physical page is zeroed across release and immediate reuse.
- Clear retained user-thread register, IPC, scheduling, fault, and diagnostic state at teardown.
- Scrub complete vCPU register, system-register, timer, interrupt, exit, and diagnostic state.
- Extend hypervisor teardown certification with poisoned-state assertions.

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
