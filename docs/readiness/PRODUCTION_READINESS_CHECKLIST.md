# Zilch Production-Readiness Checklist

Status: **Authoritative project progress tracker**  
Baseline reconciled through: **patch 0074 runtime evidence / patch 0075 documentation state**  
Scope: **Production-ready L4-style kernel and ARM64 hypervisor**  
Rule: **No profile, model, mock, bounded fixture, or self-test may be counted as product completion unless the real production mechanism exists and the required evidence is attached.**

---

## 0. Tracking rules

### Status values

- `[ ] NOT STARTED` — no production implementation exists.
- `[-] IN PROGRESS` — implementation exists but one or more required gates are incomplete.
- `[?] BLOCKED` — cannot proceed until a named dependency is completed.
- `[x] COMPLETE` — all implementation, integration, test, documentation, and evidence gates pass.
- `[!] REGRESSION` — previously completed behavior is currently broken.

### Completion rule

An item may be marked `[x] COMPLETE` only when all of the following are true:

- production code exists in the correct architectural layer;
- no test-only implementation is used as the product mechanism;
- no hard-coded fixture substitutes for runtime allocation or discovery;
- no model-only state machine substitutes for real execution;
- failure paths are implemented and tested;
- cleanup and rollback are implemented and tested;
- concurrency behavior is tested where applicable;
- security properties are enforced, not merely asserted;
- documentation describes actual behavior;
- automated evidence is retained;
- all earlier completed requirements still pass.

### Prohibited shortcuts

The following may never be used to claim completion:

- replacing real SMP execution with sequential simulation;
- replacing real guest execution with host-side state mutation;
- replacing dynamic resource management with a fixed static object pool;
- replacing capability revocation with slot invalidation only;
- replacing fault delivery with kernel-side recovery policy;
- replacing userspace servers with kernel self-test helpers;
- exposing test-only control operations in the production ABI;
- disabling security controls to make a test pass;
- broad identity mappings in final production configuration;
- writable and executable mappings in final production configuration;
- ignoring rollback, teardown, or reuse safety;
- calling a compile-only backend supported;
- calling a bounded QEMU test production certification;
- marking a feature complete without its required evidence artifacts.

### Required evidence per completed item

Every completed requirement must link to:

- implementation commit or patch;
- design section;
- unit test;
- integration test;
- negative/failure test;
- stress or concurrency test where applicable;
- runtime log or machine-readable result;
- coverage or static-analysis result where applicable;
- known limitations, which must be empty for mandatory production behavior.

---

# 1. Product and test separation

## 1.1 Build configuration

- [x] **PRD-001** Add `CONFIG_SELFTEST`. Evidence: `Makefile` release/certification selection.
- [x] **PRD-002** Add `CONFIG_HYPERVISOR_SELFTEST`. Evidence: `Makefile` and guarded hypervisor test entry points.
- [x] **PRD-003** Self-test dispatch, IPC fuzz decoding, modeled hypervisor tests, acceptance reporting, embedded guest fixtures, and verbose EL2 diagnostics are excluded or disabled in release builds.
- [x] **PRD-004** Ensure production kernel boots with all self-test options disabled. Runtime evidence: release boot reports `selftests=disabled`.
- [x] **PRD-005** Ensure production binary contains no profile-specific guest images or test fixtures. Evidence: release ELF symbol/string gate in batch 0079.
- [x] **PRD-006** CI defines independent ARM64 certification-boot and release jobs, plus AMD64 compile-only, ABI, boundary, sanitizer, documentation, permission, stack, and reproducibility gates.
- [ ] **PRD-019** A top-level Kconfig hierarchy generates `.config`, `auto.conf`, and `autoconf.h`; generated configuration is the sole source of `CONFIG_*` values for Make, C, C++, and assembly.
- [ ] **PRD-020** Checked-in debug and release defconfigs replace the development/certification/release variant set; debug enables tests and diagnostics, while release makes every test/debug option unavailable.
- [ ] **PRD-021** Release compiler flags exclude debug information and enable product optimization; release source/ELF gates reject tests, traces, debug strings, fixtures, and DWARF sections.
- [ ] **PRD-022** Kconfig exposes generic built-in, external, interactive, and per-sample guest enablement without making an external guest toolchain a core build dependency.
- [ ] **PRD-023** Guest examples live under `samples/guests/<name>/` with sample-local pinned fetch, nested toolchain shell, build, output, and acceptance ownership; the root release build succeeds with no sample fetched.

## 1.2 ABI cleanup

- [x] **PRD-007** Remove acceptance-report operations from the production ABI. Evidence: separate `sys::test_abi` in batch 0079.
- [x] **PRD-008** Remove worker-tick and other certification operations from the production ABI. Product process/object operations remain product ABI mechanisms.
- [x] **PRD-009** Move hypervisor self-test entry points behind test configuration. Evidence: separate certification ABI and `CONFIG_HYPERVISOR_SELFTEST` guest-object gating in batch 0079.
- [x] **PRD-010** Native ABI 1.0.0 is frozen through `sys::abi::v1`, `version.hh`, 64-bit register calling conventions, and immutable published numeric values.
- [x] **PRD-011** Compatibility, additive-minor-change, deprecation-duration, identifier-reuse, and major-version rules are documented.
- [x] **PRD-012** `abi-check` covers every public structure plus all enum widths and selected frozen values; `abi-headers-check` independently compiles every public header.

## 1.3 Module boundaries

- [x] **PRD-013** Architecture-independent VM/vCPU objects live under `kernel/hypervisor/object.hh`; ARM64 entry/exit and register handling remain under `arch/arm64`.
- [x] **PRD-014** Stage-2 translation management is isolated in `kernel/hypervisor/stage2.hh`.
- [x] **PRD-015** Virtual interrupt and timer state are isolated in `virtual_irq.hh` and `virtual_timer.hh`.
- [x] **PRD-016** VM lifecycle and VMID allocation are isolated in `lifecycle.hh` and `vmid.hh`.
- [x] **PRD-017** Hypervisor control models and guest fixtures live under test-only include/fixture paths selected only by certification builds.
- [x] **PRD-018** Boundary checks enforce 1,200-line source and 1,600-line header ceilings, forbid public-header test dependencies and parent-relative includes, and run in the production gate.

---

# 2. Kernel capability system

## 2.1 Capability representation

- [x] **CAP-001** Generation-safe object references exist. Runtime destroy/reuse evidence exists.
- [x] **CAP-002** Basic rights checks exist. Rights attenuation and negative checks are exercised.
- [x] **CAP-003** CSpaces use a two-level 4×64 radix with explicit root/leaf selector geometry.
- [x] **CAP-004** Every capability operation validates the CSpace's eight-bit guard before resolving a radix path; wrong-guard lookup is rejected.
- [x] **CAP-005** Per-leaf occupancy bitmaps and a rotating allocation hint allocate across all 256 slots without a linear occupied-slot scan.
- [x] **CAP-006** Endpoint badges are snapshotted from the invoking capability and delivered on both queued and direct rendezvous paths; internal caller identity remains confined to reply authority.
- [x] **CAP-007** Copy, mint, and IPC transfer enforce rights attenuation inside the authority transaction; escalation negatives and concurrent transfer/revoke certification pass.

## 2.2 Derivation and revocation

- [x] **CAP-008** The production contract explicitly bounds the generation-tagged derivation tree to 4,095 records and depth 64; exhaustion fails closed and final database validation covers every active record.
- [x] **CAP-009** Copy atomically records exact-generation parentage under the authority transaction; lifecycle, fuzz, and concurrent transfer/revoke workloads pass.
- [x] **CAP-010** Mint creates a rights-attenuated, badged derivation; delivery, wrong-right rejection, post-accept deletion semantics, and generation-tagged per-task endpoint badges are certified.
- [x] **CAP-011** Move atomically transfers one derivation between address-ordered locked CSpaces; lifecycle and four-CPU mutation/reuse workloads preserve the final database invariant.
- [x] **CAP-012** Delete, locked lookup snapshots, object read-side grace periods, and complete CSpace retirement form the bounded reader/mutation contract; lookup/destroy race and final slot/derivation validation pass.
- [x] **CAP-013** Descendant revoke performs a bounded two-phase mark/remove transaction across all registered CSpaces, preserving exact ancestry until discovery completes; child/grandchild, 193-descendant, reuse, and race evidence pass.
- [x] **CAP-014** Public mutation, IPC transfer, memory-authority, and process-retirement paths share one authority transaction; transfer/revoke, mapping revoke, SMP lifecycle, and lock-order evidence pass.
- [x] **CAP-015** Object destruction retires capability-authorized mappings and complete process bundles before generation reuse, then waits for remote readers; mapping, lookup/destroy, teardown, and reuse evidence pass.
- [x] **CAP-016** Object references and derivation records are generation tagged; inactive derivations cannot be reused while live children reference their exact generation, generation wrap retires the record, and deterministic certification forces a deleted-ancestor index-reuse attempt before the existing four-CPU object destroy/reuse workload.

## 2.3 Capability transfer

- [x] **CAP-017** Queued and direct call/reply paths atomically transfer batches of up to four capabilities, the explicit production ABI bound.
- [x] **CAP-018** Each element names a receiver-selected destination slot; successful call/reply batches and memory-server frame delivery certify placement.
- [x] **CAP-019** Duplicate or occupied destinations and partial-mint failures roll back the complete batch while preserving retryable reply authority; call and reply rollback evidence pass.
- [x] **CAP-020** A 4,096-operation cross-CSpace copy/lookup/delete fuzz sequence passes across guarded bitmap-allocated slots, including wrong-guard negatives and post-revoke reuse.
- [x] **CAP-021** Cross-CPU revoke-versus-transfer race passes with a post-revoke no-descendant invariant for both legal linearizations.

### Capability completion gate

- [x] **CAP-GATE** CAP-001 through CAP-021 pass for the documented bounded model, with aggregate race, revoke, rollback, teardown, reuse, and final capability-database evidence.

---

# 3. IPC and fault delivery

## 3.1 Core IPC semantics

- [x] **IPC-001** Basic send/receive path exists. Runtime IPC paths pass.
- [x] **IPC-002** Synchronous call implemented.
- [x] **IPC-003** Generation-tagged one-shot reply authority is serialized against reply, cancellation, timeout, server exit, and teardown under the bounded IPC lifecycle transaction.
- [x] **IPC-004** Reply-receive and reply-only operations commit transfer and consume reply authority in one lifecycle transaction; cross-CPU lifecycle races pass.
- [x] **IPC-005** Endpoint cancellation holds endpoint and lifecycle locks, revalidates the exact wait, and atomically removes membership; final invariants reject dead, duplicate, or conflicting queue entries.
- [x] **IPC-006** Teardown serializes blocked state, endpoint membership, and reply authority; retiring endpoints reject stale call/receive resolutions and reader grace precedes reuse.
- [x] **IPC-007** The typed bounded timeout ABI covers call, receive, and reply-receive; expiration removes queued waits, returns deterministic completion, restores donation, and performs no unbounded IRQ work.
- [x] **IPC-008** Notifications implement the selected nonblocking policy: atomic badge coalescing/consume, generation-checked IRQ binding, authority retirement, reader grace, and reuse invariants.
- [x] **IPC-009** Calls and replies atomically transfer batches of up to four capabilities under one authority transaction, with duplicate rejection and complete partial-mint rollback.
- [x] **IPC-010** Bounded out-of-line IPC transfers a frame capability with checked one-page offset/length metadata; receivers map it through normal memory-resource policy.

## 3.2 Scheduling integration

- [x] **IPC-011** Synchronous IPC donates the caller's remaining scheduling-context budget and inherited priority to the server, including nested calls.
- [x] **IPC-012** Donation propagates through a certified two-hop chain, is bounded at depth eight, and executes within the certified IPC service latency bound.
- [x] **IPC-013** Reply, timeout, cancellation, server exit, and teardown return unused donated budget and restore base priority under lifecycle-race certification.
- [x] **IPC-014** Normal completion and process teardown target only the receiver/owner CPU on ARM64 and record request-to-target-handler latency.
- [x] **IPC-015** Release checking records and enforces ARM64 instruction-footprint budgets for complete call, receive, and reply paths.
- [x] **IPC-016** Final certification requires IPC samples and enforces the architecture-counter service limit; the four-CPU workload passes.

## 3.3 Fault IPC

- [x] **IPC-017** User page faults delivered to configured pager. Two independent clients pass.
- [x] **IPC-018** A real ARM64 PL3 `udf` exception is classified as an instruction fault, delivers syndrome and faulting PC through production fault IPC, and is contained by userspace pager policy.
- [x] **IPC-019** ABI v1 freezes a four-word fault message containing kind, architecture syndrome, fault address, and instruction pointer; layout and values are compile-time checked and validated by real PL3 data and instruction faults.
- [x] **IPC-020** Pager resume maps and restarts recoverable data faults; terminate policy kills an undefined-instruction client without disrupting the pager or other service processes.
- [x] **IPC-021** Pager exit with live reply authority immediately terminates its faulting caller; queued or accepted orphaned faults retain a kernel safety deadline and terminate deterministically on expiry.
- [x] **IPC-022** A thread may own only one pending fault record; attempted nested delivery is rejected and the faulting thread is contained instead of overwriting pager authority.

### IPC completion gate

- [x] **IPC-GATE** Call/reply, batched capability transfer and rollback, donation, timeout, cancellation, notification, OOL frame grants, and fault IPC are integrated; dedicated lifecycle, SMP, reuse, mapped-grant, latency, and instruction-footprint gates pass.

---

# 4. Physical memory and address spaces

## 4.1 Boot-time memory discovery

- [x] **MEM-001** ARM64 imports bounded RAM ranges from DTB memory nodes; DTB is the selected 1.0 ARM64 firmware format and fallback inventory is rejected by certification.
- [x] **MEM-002** Kernel image, loaded DTB blob, FDT reservation-map entries, and `/reserved-memory` ranges are excluded before allocator publication.
- [x] **MEM-003** DTB bounds, cell geometry, overflow, tuple shape, nesting, reservation capacity, and post-subtraction region overlap are fail-closed.
- [x] **MEM-004** The bounded physical allocator publishes up to sixteen discontiguous regions; certification discovers two regions after subtracting the dynamically loaded DTB.
- [x] **MEM-005** Bootinfo v2 exports every allocatable region and root receives the encompassing memory-resource capability used for bounded delegation.

## 4.2 Resource objects

- [x] **MEM-006** Allocator-backed frame/page-table pools are charged through explicit memory-resource extents using the certified shared 256-node reusable metadata pool.
- [x] **MEM-007** Frame allocation is capability-authorized, constrained to owned extents, and uses deterministic sorted bounded traversal.
- [x] **MEM-008** Page-table allocation uses the same delegated extents, quota, accounting, scrubbing, and retirement protocol.
- [x] **MEM-009** Resource delegation and retype split/transfer extents transactionally, roll back partial metadata allocation, and deterministically coalesce returned ranges.
- [x] **MEM-010** Per-resource and per-task quotas/accounting enforce ownership; fragmented return, coalescing, metadata reuse, nested exhaustion, and balanced return pass.
- [x] **MEM-011** Zero memory before delegation and reuse.
- [x] **MEM-012** Generation, owner, extent, bitmap, double-release, overlapping-delegation, injected metadata failure, and invariant-signature rollback checks pass.

## 4.3 Mapping database

- [x] **MEM-013** Basic map/unmap and W^X checks exist.
- [x] **MEM-014** Each frame supports eight generation-checked mappings under serialized bounded transactions.
- [x] **MEM-015** Reverse mappings bind exact address-space generations and teardown removes only records for that generation.
- [x] **MEM-016** Unmap, revoke, release, and destroy share authority/mapping transactions; final acceptance validates counts, generations, authorities, attributes, and VA uniqueness.
- [x] **MEM-017** Normal and device mappings enforce explicit cacheability and shareability for the supported ARM64 platform.
- [x] **MEM-018** Root-authorized allowlisted MMIO frames use device attributes and reject executable or normal-memory aliases.
- [x] **MEM-019** Process teardown switches to the permanent kernel root, retires resources, unmaps, drains the CSpace, revokes the bundle, waits for readers, and passes SMP reuse stress.
- [x] **MEM-020** SMP TLB shootdown implemented and runtime verified on four CPUs.
- [x] **MEM-021** Generation-tagged ASID allocation performs global stage-1 invalidation on rollover, lazily refreshes stale live address spaces, and ignores stale-generation releases; certification rolls over before real PL3 execution.

## 4.4 User pager integration

- [x] **MEM-022** Every created task receives an explicit pager endpoint and the selected 1.0 policy validates the recorded fault page and access before resume.
- [x] **MEM-023** Fault IPC carries fault address, access syndrome/type data, and PC.
- [x] **MEM-024** Pager map/resume is bound to the recorded fault page and access type; wrong-page and insufficient-permission replies are rejected without consuming reply authority, corrected retry succeeds, and terminate policy is runtime verified.
- [x] **MEM-025** Fault map/reply is lifecycle/mapping serialized and identical simultaneous outcomes are idempotent with one mapping record.
- [x] **MEM-026** Orphaned faults have a bounded safety deadline; pager exit consumes live fault reply authority and the selected containment policy terminates rather than silently reassigning.

### Memory completion gate

- [x] **MEM-GATE** Discovered allocatable RAM is delegated, mapped, revoked, faulted, reclaimed, and reused through bounded production paths; fallback inventory is rejected and the dedicated memory completion gate passes.

---

# 5. Scheduler and real-time behavior

## 5.1 Scheduler core

- [x] **SCH-001** Basic SMP runnable scheduling exists and root-created workers run on CPUs 1–3.
- [x] **SCH-002** Per-CPU selection scans the fixed ten-thread production table, chooses the highest effective priority deterministically, and passes four-CPU workload and latency gates.
- [x] **SCH-003** Deterministic priority ordering implemented.
- [x] **SCH-004** Threads have explicit affinity; suspended, donation-free threads may migrate transactionally through scheduling reconfiguration, while active migration is rejected.
- [x] **SCH-005** Creation and targeted IPC wakeups issue reschedule IPIs; four-CPU execution and bounded request-to-receipt telemetry pass.
- [x] **SCH-006** CPU hotplug/offline is explicitly unsupported for 1.0; the online CPU set is immutable after boot.

## 5.2 Scheduling contexts

- [x] **SCH-007** Scheduling-context objects exist.
- [x] **SCH-008** Budget charging, throttling, quiescent priority/budget/period/affinity reconfiguration, and long-horizon logical-time stress pass.
- [x] **SCH-009** Bounded replenishment exists; configuration rejects zero budget, zero period, budget greater than period, and deadlines that overflow the logical timebase.
- [x] **SCH-010** Per-slice sporadic replenishment uses real scheduler timestamps and a bounded ordered queue with bandwidth-safe overflow coalescing; staggered return, throttle, overflow, donation, eventual-progress, pager-liveness, and full acceptance certification pass.
- [x] **SCH-011** Scheduling-context budget and effective priority donation are integrated with synchronous IPC and deterministic unwind.
- [x] **SCH-012** Donation chains propagate budget and inherited priority and reject depth beyond eight.
- [x] **SCH-013** A lower-priority server executes at the caller's inherited priority until reply, cancellation, timeout, exit, or teardown.
- [x] **SCH-014** Per-CPU absolute-deadline timeout queues replace whole-thread scans; entries are generation checked and timer expiry never spins on the IPC lifecycle lock.

## 5.3 RT correctness

- [x] **SCH-015** Every active blocking kernel spinlock has a documented global rank; equal-rank CSpace locks use increasing address order and releases are strict LIFO.
- [x] **SCH-016** Generation-safe lock-order instrumentation records and reports the maximum hold duration in architectural timer ticks.
- [x] **SCH-017** Scoped IRQ-disabled logging and timeout-queue sections retain per-CPU samples and maxima and remain below the 10 ms QEMU certification-profile bound; hardware qualification remains a separate release gate.
- [x] **SCH-018** Logging has an RT-safe structured deferred path through `printk::defer`; formatted asynchronous draining remains OBS-003.
- [x] **SCH-019** Active CPUs retain a one-tick scheduling quantum while idle CPUs program the next timeout deadline or a bounded one-second housekeeping deadline.
- [x] **SCH-020** IRQ service duration has nonzero samples and remains below the certification-profile 10 ms bound through the full workload.
- [x] **SCH-021** Timer-driven preemption service has nonzero samples and remains below the certification-profile 10 ms bound.
- [x] **SCH-022** Cross-CPU wake request-to-reschedule-IPI receipt has nonzero samples and remains below the certification-profile 10 ms bound.
- [x] **SCH-023** IPC syscall service has nonzero samples and remains below the certification-profile 10 ms bound across pager, memory-server, lifecycle, and SMP workloads.
- [x] **SCH-024** Accelerated deterministic stress advances six logical hours and 21,600 one-second sporadic periods without deadline, throttle, replenishment, or accounting violation.

### Scheduler completion gate

- [x] **SCH-GATE** The fixed-capacity four-CPU scheduler passes budget, replenishment, donation, priority inheritance, quiescent migration, timeout ordering, logical-time soak, state invariants, and all certification latency limits.

---

# 6. Interrupts, timers, and platform support

## 6.1 ARM64 interrupt subsystem

- [x] **IRQ-001** GICv3 distributor and CPU interfaces initialize on QEMU ARM64 virt.
- [x] **IRQ-002** An atomic exclusive IRQ registry publishes one generation-checked object per supported SPI, rejects reserved/private lines, rolls back failed configuration, and provides acquire/release dispatch/unregister ordering.
- [x] **IRQ-003** Registered IRQ capabilities support rights-attenuated cross-CSpace delegation and revoke; guarded delegation and post-revoke rejection pass for the bounded platform contract.
- [x] **IRQ-004** GIC mask/unmask, priority-drop, explicit deactivate, active-state validation, notification-gated acknowledge semantics, and mask-before-rebind with active rejection are implemented.
- [x] **IRQ-005** Edge and level SPIs are configured, delivered, notification-signaled, explicitly deactivated, and acknowledged in certification; the real architectural timer remains level-triggered.
- [x] **IRQ-006** IRQ ownership is exclusive for 1.0: a second object cannot register the same physical line; shared-line demultiplexing is delegated to a userspace driver service.
- [x] **IRQ-007** A bounded delivery window masks a line after 64 events and requires explicit rebinding/recovery before delivery resumes.
- [x] **IRQ-008** Per-IRQ delivered, acknowledged, suppressed, window-count, active, masked, and stormed diagnostics are maintained.

## 6.2 Timers

- [x] **TIM-001** Architectural virtual timer initializes and per-CPU progress is verified.
- [x] **TIM-002** Each ARM64 CPU programs its local virtual timer from an absolute scheduler deadline.
- [x] **TIM-003** The head of each per-CPU timeout queue drives idle timer programming without losing elapsed logical ticks.
- [x] **TIM-004** Counter frequency, hardware interval bounds, zero-delay behavior, and deadline-addition overflow are validated and fail closed.
- [x] **TIM-005** Suspend/resume is explicitly out of scope for 1.0; timer and scheduler state assume one uninterrupted boot.

### Interrupt, timer, and platform completion gate

- [x] **ITP-GATE** The QEMU ARM64 profile passes GIC initialization, reserved-line exclusion, exclusive edge/level IRQ lifecycle, delegation/revoke, storm containment, per-CPU timer progress/deadlines, targeted IPIs, platform inventory, final databases, SMP stress, teardown, and reuse.

## 6.3 Platform support

- [x] **PLT-001** QEMU ARM64 virt is the complete versioned 1.0 virtual-platform profile.
- [x] **PLT-002** Real ARM64 hardware qualification is explicitly separated from the virtual-platform gate and remains a distinct blocking release gate.
- [x] **PLT-003** The profile imports RAM/reservations from DTB and validates its versioned fixed QEMU MMIO, GIC, CPU-count, timer, and userspace-SPI inventory at final acceptance.
- [x] **PLT-004** The 1.0 virtual profile deliberately retains the polling UART as a kernel diagnostic console; device-driver ownership transfer is outside this root-only profile.
- [x] **PLT-005** Watchdog hardware is explicitly unsupported by the QEMU 1.0 profile; bounded kernel panic/emergency diagnostics are the selected failure policy.
- [x] **PLT-006** Reset and power-off are explicitly unsupported kernel operations for the QEMU 1.0 profile; runs terminate through the external machine controller.

## 6.4 AMD64 truthfulness

- [x] **PLT-007** Mark AMD64 compile-only until runtime backend exists.
- [x] **PLT-008** AMD64 IDT/exception runtime is explicitly absent and excluded from the ARM64 1.0 platform claim.
- [x] **PLT-009** AMD64 APIC/interrupt routing is explicitly absent; no runtime support is advertised.
- [x] **PLT-010** AMD64 SMP startup is explicitly absent; AMD64 remains compile-only.
- [x] **PLT-011** AMD64 page-table runtime is explicitly absent; compile compatibility does not imply boot support.
- [x] **PLT-012** AMD64 timer runtime is explicitly absent; release checks are compile/ELF checks only.
- [x] **PLT-013** AMD64 virtualization is explicitly deferred beyond 1.0 together with the compile-only AMD64 runtime.

---

# 7. Userspace control-plane OS

## 7.1 Root resource manager

- [x] **USR-001** PL3 root task boots.
- [-] **USR-002** Root receives memory inventory metadata and existing bootstrap capabilities; explicit capability delegation for all allocatable RAM remains open.
- [x] **USR-003** Production root contains no kernel acceptance-test policy; the acceptance runner is excluded by `CONFIG_SELFTEST`.
- [-] **USR-004** Production root launches the memory server plus an independently linked five-role PL3 service graph, retains lifecycle authority, and actively probes private health endpoints; crash recovery remains open.
- [-] **USR-005** Versioned memory allocation IPC and per-role health/description IPC expose bounded policy; a unified external root management endpoint remains open.

## 7.2 Memory server and pager

- [-] **USR-006** The independently linked PL3 memory server runs a persistent production request loop and allocates frames through its delegated memory-resource capability; inventory/policy APIs remain open.
- [-] **USR-007** Root bootinfo carries the physical memory inventory; the userspace memory server does not yet import and manage it.
- [-] **USR-008** The production PL3 memory server provides resource-backed allocation/query/release and transfers derived frame capabilities into client-selected slots; asynchronous queues and scalable handle management remain open.
- [-] **USR-009** Independent pager service handles two sequential clients; concurrency, death, and pressure policies remain open.
- [ ] **USR-010** Demand paging implemented where configured.
- [-] **USR-011** Resource quota exhaustion returns deterministic `no_memory`; reclamation/pressure policy remains open.
- [-] **USR-012** Bounded per-task memory-resource quotas are created at process construction and enforced for resource-backed objects; configurable domain policy remains open.

## 7.3 Process and ELF loader

- [ ] **USR-013** General ELF64 loader not yet implemented; current binaries use a controlled bootstrap image registry.
- [ ] **USR-014** General ELF segment permission validation not yet implemented.
- [ ] **USR-015** TLS, stack, and initial process image setup implemented.
- [ ] **USR-016** Dynamic linker support plan implemented or explicitly deferred.
- [-] **USR-017** Kernel process-bundle create/destroy exists; path-based userspace process-manager API remains open.
- [ ] **USR-018** Process crash reporting implemented.

## 7.4 Device and IRQ management

- [ ] **USR-019** Device resource database implemented.
- [ ] **USR-020** MMIO delegation implemented.
- [ ] **USR-021** IRQ broker implemented.
- [ ] **USR-022** Userspace UART driver implemented.
- [ ] **USR-023** Console server implemented.
- [ ] **USR-024** Driver crash and restart policy implemented.

## 7.5 Domain manager

- [-] **USR-025** Userspace domain manager/VMM implemented; PL3 domain-manager service, role-specific image loading, a dedicated load operation, VM launch/destroy request handling, and earlyfs packaging exist, but production guest deployment remains open.
- [ ] **USR-026** VM creation uses capability-authorized kernel APIs.
- [ ] **USR-027** Guest image loading performed in userspace.
- [ ] **USR-028** VM memory and device assignment policy remains in userspace.
- [-] **USR-029** VM lifecycle exposed through stable management API; `sys::domain_manager::manager`, the domain-role control-plane request path, and the dedicated load op cover create/destroy in certification, but the production management protocol remains open.
- [ ] **USR-030** Linux guest launch demonstrated.
- [ ] **USR-031** BSD guest launch demonstrated.
- [x] **USR-032** Pinned Zephyr v4.0.0 boots through the PL3 domain manager with section-level W^X loading, bounded vGIC/timer support, delegated PL011, and a native interactive shell that accepts `help` and returns the command list.

## 7.6 Supervision

- [-] **USR-033** Root launches the bounded six-service graph, retains lifecycle capabilities, monitors readiness/failure badges, and actively probes every private service endpoint; exit-status monitoring remains open.
- [-] **USR-034** Bounded per-role restart admission and real stop/destroy/recreate/remint/health recovery are implemented; unexpected-fault-triggered production restart remains open.
- [x] **USR-035** Core roles have an explicit bounded dependency mask and root launches process, device, console, domain, then supervisor in dependency order.
- [-] **USR-036** Per-role restart limits fail closed and zero-limit services cannot restart; time-windowed backoff and unexpected crash accounting remain open.
- [-] **USR-037** Every core role publishes a collision-free readiness badge and answers a role-bound health/description RPC; asynchronous fault records remain open.

### Userspace completion gate

- [ ] **USR-GATE** The control-plane OS is complete only when the kernel boots a real management-domain service graph and all core policies execute in userspace.

---

# 8. Hypervisor core

## 8.1 VM and vCPU object model

- [x] **HYP-001** Capability-authorized dynamic VM/vCPU objects use generation-checked bounded pools and the production `sys::vmm::machine` userspace orchestration layer.
- [x] **HYP-002** Every dynamic VM receives a generation-tagged VMID and allocator-backed, page-aligned, scrubbed stage-2 hierarchy.
- [x] **HYP-003** Capability-authorized create/configure/state/run/pause/resume/reset/stop/destroy, frame-backed load/unload, and userspace VMM orchestration are exposed.
- [x] **HYP-004** Per-VM accounting tracks current/peak mapped pages, map/unmap totals, active vCPUs, and run entry/exit balance with overflow/underflow fault detection.
- [x] **HYP-005** The vCPU context saves GPRs, PC/PSTATE, EL1 translation, exception, TLS, timer, and virtual-GIC state and exposes bounded state read/write operations.
- [x] **HYP-006** PSTATE, SCTLR, TCR, CPACR, CNTKCTL, translation bases, and unsupported system-register accesses are masked, aligned, emulated, or rejected.
- [x] **HYP-007** Per-VM locking and active-run accounting serialize teardown against concurrent execution and reject destruction until all vCPUs quiesce.

## 8.2 Stage-2 translation

- [x] **HYP-008** Stage-2 map/unmap populate real ARM64 descriptors with W^X, device, overlap, alignment, active-run, and accounting enforcement.
- [x] **HYP-009** Dynamic VMs allocate and reclaim scrubbed L1/L2/L3 tables from the physical-page allocator with transactional hierarchy rebuild.
- [x] **HYP-010** Stage-2 permissions reject empty, write-only, W+X, executable-device, and unknown-bit combinations, and require the frame capability's normal/device type to match.
- [x] **HYP-011** Conservative access/dirty tracking is generation-tagged and queryable/clearable through the capability ABI.
- [x] **HYP-012** `vcpu_run` returns stage-2 fault reason, ESR, FAR, guest PC, and reconstructed IPA through the stable six-word userspace exit result.
- [x] **HYP-013** Generation-tagged VMID allocation performs global stage-2 invalidation on rollover, lazily refreshes stale live VMs before mapping/reset/run, and ignores stale-generation releases.
- [x] **HYP-014** Map/unmap and run share the per-VM transaction lock; certification covers active execution, teardown rejection, hierarchy rebuild, and SMP execution.
- [x] **HYP-015** Public stage-2 mapping accepts only an authorized frame capability, never a physical address; frame type and permissions are validated before mapping.

## 8.3 Real multi-vCPU execution

- [x] **HYP-016** Real single-vCPU EL2/EL1/EL0 execution works.
- [x] **HYP-017** Secondary guest entry streams execute independently through EL2.
- [x] **HYP-018** Four physical CPUs concurrently run four real guest vCPUs.
- [x] **HYP-019** A guest-side `LDAXR`/`STLXR` SMP barrier completes with four participants.
- [x] **HYP-020** Full vCPU architectural, timer, exit, and virtual-GIC state is saved on every bounded exit.
- [x] **HYP-021** All four vCPUs resume on different physical CPUs with independent state.
- [x] **HYP-022** Migration executes with generation-tagged VMIDs and global stage-1/stage-2 TLB maintenance.
- [x] **HYP-023** Two VMs and four vCPUs execute concurrently through EL2 with independent barriers and stage-2 roots.
- [x] **HYP-024** An unmapped-instruction crash in one VM leaves both peer-VM vCPUs runnable and uncorrupted.

## 8.4 Virtual interrupt controller

- [x] **HYP-025** The bounded virtual interrupt controller is vCPU-resident and integrated with real guest delivery.
- [x] **HYP-026** Production virtual-GIC state includes VMCR, HCR, AP registers, list registers, priorities, triggers, and lifecycle accounting.
- [x] **HYP-027** SGI, PPI, and SPI classes share validated injection and delivery semantics.
- [x] **HYP-028** Priority selection, PMR masking, per-IRQ masking, and deterministic tie-breaking are implemented.
- [x] **HYP-029** Level assertion/re-pend and edge one-shot semantics are implemented.
- [x] **HYP-030** Pending, active, acknowledge, deactivate, and re-pend transitions are certified.
- [x] **HYP-031** GIC maintenance PPI handling queries `ICH_MISR_EL2` through the EL2 service.
- [x] **HYP-032** ARM GIC virtualization list registers are saved, restored, and used where available.
- [x] **HYP-033** The HCR-based software fallback exposes the same guest interrupt acknowledgement contract.

## 8.5 Virtual timers

- [x] **HYP-034** Basic virtual timer event injection executes through the real ARM64 guest path.
- [x] **HYP-035** `CNTV_CTL_EL0` is saved at guest exit, retained in the vCPU context, and restored on re-entry; the real guest arms it before WFI and certification validates the captured enabled state.
- [x] **HYP-036** `CNTV_CVAL_EL0` is saved/restored with the vCPU context; certification validates the nonzero guest deadline and production timer-state synchronization.
- [x] **HYP-037** Each VM owns a virtual counter offset that is programmed into `CNTVOFF_EL2` on entry, read back, and replaced with the saved host value on every normal or rejected exit; the real guest runs with a nonzero offset.
- [x] **HYP-038** Descheduled vCPU deadlines participate in host deadline programming; timer IRQ polling queues one event and wakes blocked vCPUs.
- [x] **HYP-039** Timer state is vCPU-resident and retained across real cross-CPU migration.
- [x] **HYP-040** Certification covers 1,024 expiry/acknowledge/re-arm/cancel interleavings plus real migrated timer state.

## 8.6 Hypercalls and exits

- [x] **HYP-041** The versioned guest hypercall contract provides console, time, IRQ acknowledgement, report, diagnostic, shutdown, and bounded unknown-call exits.
- [x] **HYP-042** ABI v1 defines capability-authorized lifecycle calls and a fixed six-word exit result carrying reason, syndrome, FAR, PC, and qualification.
- [x] **HYP-043** Unknown guest hypercalls exit to the host as bounded `hypercall` exits with the rejected call number preserved in the qualification field.
- [x] **HYP-044** Valid stage-2 data aborts produce userspace MMIO exits encoding IPA, direction, width, sign extension, and target register.
- [x] **HYP-045** Trapped WFI/WFE advances the guest PC and returns a bounded `wait` exit with the original ESR for userspace scheduling policy.
- [x] **HYP-046** Trapped SCTLR, TCR, TTBR0/1, and MAIR reads/writes are sanitized and emulated; unsupported encodings exit to userspace.
- [x] **HYP-047** Guest abort exits record ESR, FAR, guest PC, and reconstructed IPA from HPFAR/FAR; negative certification validates abort classification and IPA reconstruction.

### Hypervisor execution gate

- [x] **HYP-EXEC-GATE** Runtime certification executes four guest instruction streams concurrently through EL2 on four physical CPUs and passes guest-side atomic barrier, interrupt, migration, and timer-state tests.

---

# 9. Device assignment and SMMU/IOMMU

## 9.1 Device resource model

- [ ] **DEV-001** Device ownership represented by capabilities.
- [ ] **DEV-002** MMIO regions delegated safely.
- [ ] **DEV-003** IRQs delegated and revoked safely.
- [ ] **DEV-004** Device reset requirement documented per device.
- [ ] **DEV-005** Assignment rollback implemented.

## 9.2 SMMU

- [ ] **DEV-006** ARM SMMU discovery implemented.
- [ ] **DEV-007** Stream ownership database implemented.
- [ ] **DEV-008** Per-domain translation context implemented.
- [ ] **DEV-009** DMA mappings tied to VM capabilities.
- [ ] **DEV-010** SMMU invalidation and synchronization implemented.
- [ ] **DEV-011** DMA blocked before memory/device reuse.
- [ ] **DEV-012** Fault reporting and containment implemented.
- [ ] **DEV-013** Interrupt remapping implemented where required.

## 9.3 Production assignment tests

- [ ] **DEV-014** Assigned device cannot DMA into kernel memory.
- [ ] **DEV-015** Assigned device cannot DMA into another VM.
- [ ] **DEV-016** Device revoke stops DMA before teardown completes.
- [ ] **DEV-017** Device reset prevents state leakage to the next owner.
- [ ] **DEV-018** Faulting device does not crash the host.

### Device-assignment gate

- [ ] **DEV-GATE** Direct assignment is production-ready only after SMMU isolation, DMA quiescence, reset, revoke, and failure rollback all pass.

---

# 10. Security and hardening

## 10.1 Memory protection

- [x] **SEC-001** Kernel, user, and guest mappings enforce W^X, with bootstrap page-table certification and negative mapping tests.
- [x] **SEC-002** Every production TTBR0 root shares page-granular kernel RX/RO-NX/RW-NX mappings; user and guest mapping APIs reject writable-executable permissions.
- [x] **SEC-003** Embedded images and kernel rodata are mapped EL1 read-only and non-executable after MMU initialization; bootstrap validates every image-window PTE.
- [x] **SEC-004** Every EL1 and EL2 per-CPU stack has an unmapped guard page below its 32 KiB usable region, plus exception-time bounds/canary checks and retained high-water marks. Bootstrap certification verifies all guard and adjacent usable PTEs.
- [x] **SEC-005** User copy validates overflow, the complete page range, EL0 permissions, write permission, and the kernel boundary before unprivileged `LDTRB`/`STTRB` access; certification covers valid, read-only, unmapped, crossing, and wrapping ranges.
- [x] **SEC-006** Reusable physical pages are scrubbed on release and allocation; user-thread and vCPU architectural, IPC, timer, interrupt, exit, and diagnostic state is cleared before slot reuse. Certification poisons and verifies both page and vCPU reuse boundaries.

## 10.2 Architecture hardening

- [x] **SEC-007** Host MAIR/TCR use audited constructed constants, SCTLR enables M/C/I/WXN with little-endian enforcement, final acceptance checks architectural readback, and hostile guest SCTLR values are masked with mandatory RES1 restoration.
- [x] **SEC-008** Every online CPU publishes CSV2/CSV3/SSBS/PAuth/BTI inventory and validation boundaries execute CSDB+ISB; the complete QEMU profile inventory is required at final acceptance while hardware qualification remains separate.
- [x] **SEC-009** Pointer authentication was evaluated and is explicitly deferred until all C++ and hand-written exception/boot/guest-entry paths can be signed and negatively tested together.
- [x] **SEC-010** BTI was evaluated and is explicitly deferred until every indirect target, vector, context-switch, and guest-entry assembly path has audited landing pads.
- [x] **SEC-011** PAN is enabled and UAO disabled on CPUs advertising each extension, with bootstrap readback verification; unsupported baseline Armv8-A CPUs safely skip optional instructions.
- [x] **SEC-012** Release kernels exclude IPC fuzz/debug decoding, deny the guest diagnostic hypercall, omit detailed EL2 console walks, and retain only bounded production diagnostics.

## 10.3 Concurrency hardening

- [x] **SEC-013** Endpoint, IPC lifecycle, capability, mapping, allocator, and object-table locks follow the documented global hierarchy.
- [x] **SEC-014** Certification builds check per-CPU acquisition rank, recursion, equal-rank address order, depth, and reverse release; the full four-CPU suite reports zero violations.
- [x] **SEC-015** Object/VM counters reject saturation, underflow, and imbalance; final object, capability, mapping, process, scheduler, endpoint, notification, interrupt, timer, and memory invariants expose lifecycle drift.
- [x] **SEC-016** Generation-tagged objects, derivations, ASIDs, VMIDs, reply authority, endpoints, notifications, interrupts, and CPU bindings fail stale references closed; forced ABA and rollover/reuse certification pass.
- [x] **SEC-017** Thread/process, address-space, IPC, IRQ, VM/vCPU, frame/page-table, notification, and capability teardown protocols retire authority, quiesce execution/readers, clear state, and pass reuse invariants.
- [x] **SEC-018** The bounded concurrency matrix covers transfer/revoke, lookup/destroy, IPC cancel/timeout/exit/teardown, mapping-authority revoke, IRQ active/mask/rebind, VM/vCPU execution/teardown, SMP fuzz, and final database checks.

## 10.4 Failure handling

- [x] **SEC-019** Fatal exception and stack-corruption handling masks all exception classes and records through lock-free emergency storage without consulting scheduler, allocator, capability, object, or console-lock state; certification poisons scheduler identity and holds printk locked while validating capture.
- [x] **SEC-020** Each CPU has a lock-free 32-record emergency ring for exception entry, fatal traps, stack corruption, and bounded-printk contention.
- [x] **SEC-021** Fatal exceptions preserve a checksummed EL/vector/ESR/FAR/PC crash record in a linker-reserved `.noinit` page excluded from BSS clearing.
- [x] **SEC-022** The QEMU 1.0 profile has no watchdog device; this is explicit and fail-closed, while fatal paths preserve lock-free emergency and checksummed crash state for the external machine controller.
- [x] **SEC-023** Recoverable user instruction/data faults are delivered through fault IPC or isolate only the faulting thread; pager recovery and continued four-CPU acceptance prove the kernel remains live.
- [x] **SEC-024** Guest traps always return through bounded VM exits; unexpected traps fault only the owning vCPU/VM, while stage-2 faults remain recoverable VMM exits.

### Security and hardening completion gate

- [x] **SEC-GATE** SEC-001 through SEC-024 pass for the documented QEMU ARM64 threat model with architectural readback, W^X/WXN, stack/user-copy protection, checked lifecycle databases, race/reuse evidence, failure-state integrity, release binary audits, and aggregate certification.

---

# 11. Observability and diagnostics

- [x] **OBS-001** Structured kernel log levels exist.
- [x] **OBS-002** `printk` disables local IRQs and uses bounded lock acquisition; contention records to the per-CPU emergency ring instead of spinning behind an interrupted owner.
- [-] **OBS-003** Contended records are deferred into lock-free per-CPU rings, but formatted asynchronous draining remains open.
- [x] **OBS-004** Fixed-size lock-free per-CPU event rings retain exception and emergency trace records with release-published sequence numbers.
- [x] **OBS-005** Versioned per-CPU records trace IPC entry, scheduler switches, IRQs, VM exits, user faults, and exception entry.
- [x] **OBS-006** Routine tracing is compiled out when `CONFIG_TRACE=0` in release builds; fatal and contention records remain always enabled.
- [x] **OBS-007** Per-type object live/peak/create/destroy counters and per-VM mapping/run counters expose lifecycle imbalance without allocation.
- [-] **OBS-008** A release-enabled, sequence-published bounded ring audits VM reset, mapping, run, pause/resume, stop, and teardown; device-assignment records remain open.
- [x] **OBS-009** Emergency record format version 1, event identifiers, publication rules, and field meanings are documented.
- [x] **OBS-010** Release logs exclude guest registers and user/guest PC, FAR, ESR, and IPA details; verbose diagnostics are restricted to development/certification builds.
- [ ] **OBS-011** Formatted kernel records use Linux-style boot-relative `[    seconds.microseconds]` timestamps from the calibrated architectural counter; SMP record serialization includes the timestamp and severity prefix.

---

# 12. Testing and verification

## 12.1 Host testing

- [x] **TST-001** Portable capability and scheduling-context logic builds and executes as a native host test independently of the freestanding kernel image.
- [x] **TST-002** Host capability tests exhaustively verify all 64 bounded rights masks for attenuation and empty-slot rejection; runtime certification supplies derivation, revoke, race, and reuse coverage.
- [ ] **TST-003** IPC state-machine unit tests implemented.
- [x] **TST-004** Native scheduling tests cover 65,536 deterministic charge/replenish operations, invariants, donation, inheritance, and unwind in addition to runtime sporadic certification.
- [ ] **TST-005** VM lifecycle unit tests implemented.
- [ ] **TST-006** Stage-2 table unit tests implemented.
- [x] **TST-007** The host suite executes deterministic generated rights and scheduling state sequences and checks bounds, ordering, accounting, and donation properties.
- [x] **TST-008** Host kernel logic runs under ASan+UBSan with recovery disabled; ABI layout has its independent UBSan gate.

## 12.2 Runtime integration testing

- [x] **TST-009** QEMU ARM64 smoke and bounded acceptance tests exist.
- [x] **TST-010** Production configuration boots with self-tests disabled.
- [ ] **TST-011** Real userspace service graph integration test exists.
- [x] **TST-012** Fault IPC and two-client pager integration test exists.
- [ ] **TST-013** Real multi-vCPU guest integration test exists.
- [ ] **TST-014** Concurrent two-VM execution test exists.
- [ ] **TST-015** Device assignment and SMMU integration test exists.

## 12.3 Stress and fuzzing

- [x] **TST-016** Deterministic bounded kernel/hypervisor fuzz exists. It is evidence for bounded mechanisms, not production completion.
- [x] **TST-017** Capability certification combines 4,096 generated cross-CSpace operations, 128 derive/revoke cycles, a 193-descendant bulk revoke, deterministic generation ABA, and cross-CPU transfer/revoke.
- [-] **TST-018** `ipc_lifecycle_races` covers explicit cancel, timer expiry, blocked destroy, server exit with live reply authority, and endpoint reuse across CPUs; controlled instruction-level race fuzz and fault injection remain open.
- [-] **TST-019** Deterministic capability-revoke-driven unmapping integration test exists; concurrent revoke/map/unmap and TLB-shootdown race fuzz remain open.
- [ ] **TST-020** Scheduler migration/preemption race fuzz implemented.
- [x] **TST-021** Deterministic VM lifecycle stress forces VMID rollover with a live bootstrap VM, refreshes it, then passes real guest execution, teardown, reuse, and concurrent lifecycle models.
- [ ] **TST-022** Virtual interrupt storm test implemented.
- [-] **TST-023** Certification exercises allocation/accounting, nested extent split/retype/reclaim, twenty-way fragmentation/coalescing, metadata reuse, 32 repeated quota-exhaustion/reclaim cycles covering 512 frame lifecycles, balanced release, multi-map cleanup, attributes, and MMIO lifecycle; full allocator exhaustion and multi-CPU pressure remain open.
- [-] **TST-024** Attribute/mapping rejection and injected extent-split metadata failure verify transactional rollback with before/after invariant signatures; systematic injection at object registration, capability installation, frame/page-table allocation, and teardown remains open.

## 12.4 Long-duration certification

- [ ] **TST-025** 24-hour kernel SMP soak passes.
- [ ] **TST-026** 24-hour multi-VM soak passes.
- [ ] **TST-027** 72-hour mixed workload soak passes.
- [ ] **TST-028** Repeated reboot and lifecycle test passes.
- [-] **TST-029** Certification proves object create/destroy and VM map/unmap/run counters return to balance across bounded lifecycle suites; long-duration soak evidence remains open.
- [ ] **TST-030** No missed deadlines under defined RT workload.

## 12.5 Static verification

- [x] **TST-031** The Clang analyzer profile is clean for the portable host-tested kernel logic; freestanding cross-architecture assembly/MMIO paths are explicitly covered by compile, ELF, runtime, and invariant gates instead.
- [x] **TST-032** `clang-tidy` runs the `clang-analyzer-*` profile with warnings as errors over the portable kernel test translation unit in CI and locally.
- [x] **TST-033** UBSan runs over the portable ABI layout test with recovery disabled; architecture-specific freestanding kernel code remains outside host sanitizer scope.
- [x] **TST-034** Release builds emit compiler stack-usage records; ARM64 and AMD64 production gates reject any function exceeding the 8 KiB bound against 32 KiB per-CPU stacks.
- [x] **TST-035** Release ELF section flags are audited for W+X sections, executable text, and non-writable rodata on both supported build profiles.
- [x] **TST-036** Reproducible release builds are verified byte-for-byte for ARM64 and AMD64 ELF, raw image, userspace ELF/map, and early filesystem artifacts with a fixed source epoch; map paths are normalized before comparison.
- [x] **TST-037** Host verification emits an LLVM source/region/function/line/branch coverage report; the current gate records 100% function and greater than 90% line coverage for the host driver.

---

# 13. Documentation and architecture conformance

- [ ] **DOC-001** `arch_design.md` reflects implemented architecture.
- [ ] **DOC-002** `detail_design.md` reflects implemented mechanisms.
- [x] **DOC-003** Every mandatory requirement has a stable ID in this checklist.
- [ ] **DOC-004** Every requirement maps to implementation and tests.
- [-] **DOC-005** Model-only runtime results now use `HV-MODEL` and `hypervisor_control_model`; legacy profile documents still require complete renaming and archival.
- [x] **DOC-006** Unsupported 1.0 kernel and platform features are explicitly documented with non-partial-mutation rules.
- [x] **DOC-007** Kernel threat model and excluded physical, firmware, timing, and pre-SMMU DMA threats are documented.
- [x] **DOC-008** EL1, PL3, guest, capability, stage-2, firmware, root-policy, and device trust boundaries are documented.
- [ ] **DOC-009** Capability and IPC semantics documented formally enough for independent implementation.
- [x] **DOC-010** Lock, atomic publication, emergency/audit ring, page-table/TLBI, MMIO, and reclamation ordering rules are documented.
- [-] **DOC-011** Kernel lock ordering and major object/user-thread/VM teardown protocols are documented; IRQ and device teardown protocols remain open.
- [ ] **DOC-012** Hypervisor guest-visible architecture documented.
- [ ] **DOC-013** Userspace server APIs documented.
- [x] **DOC-014** Semantic release classes, ABI/diagnostic compatibility, deprecation, migration, and mandatory release gates are documented.

---

# 14. Production release gates

## Kernel 1.0 gate

The kernel may be called **production-ready** only when all of these gates are complete:

- [x] Product/test separation gate
- [x] Capability completion gate
- [x] IPC completion gate
- [x] Memory completion gate
- [x] Scheduler completion gate
- [x] Interrupt and timer production gate
- [ ] Userspace control-plane gate
- [x] Security and hardening gate
- [ ] Verification and soak gate
- [ ] Documentation and conformance gate
- [ ] Real hardware ARM64 certification gate

The bounded kernel mechanisms have an independently executable core gate:

- [x] **KERNEL-CORE-GATE** Capability, IPC, memory, scheduler, interrupt/timer/platform, and security gates compose into `kernel_core_1_0_gate` and final kernel invariants with zero certification failures.

The overall Kernel 1.0 production-ready claim remains blocked by the unchecked
userspace, verification/soak, documentation/conformance, and real-hardware
gates above.

## Hypervisor 1.0 gate

The hypervisor may be called **production-ready** only when all of these gates are complete:

- [ ] Hypervisor object/lifecycle gate
- [ ] Stage-2 translation gate
- [ ] Real multi-vCPU execution gate
- [ ] Production virtual interrupt gate
- [ ] Production virtual timer gate
- [ ] Concurrent multi-VM gate
- [ ] Userspace VMM/domain-manager gate
- [ ] Device assignment and SMMU gate
- [ ] Guest fault-containment gate
- [ ] Security and teardown gate
- [ ] Stress, fuzz, and soak gate
- [ ] Real hardware ARM64 certification gate

## Final release evidence

- [ ] Requirements matrix has no mandatory open items.
- [ ] No test-only code is linked into production images.
- [ ] No known critical or high-severity security defect remains.
- [ ] No mandatory feature relies on a model, mock, fixture, or hard-coded resource pool.
- [ ] All supported platform claims are runtime tested.
- [ ] Reproducible release artifacts are generated.
- [ ] Release source, toolchain versions, configuration, and test evidence are archived.
- [ ] Independent review signs off kernel and hypervisor separately.

---

# 15. Immediate execution order

## Phase A — restore architectural discipline

- [x] A1. Add test-only configuration boundaries.
- [-] A2. Test operations are configuration-guarded; final production ABI cleanup and binary audit remain open.
- [ ] A3. Split hypervisor implementation into production modules.
- [-] A4. Stable requirement IDs now exist; implementation/test/evidence links must be populated.
- [ ] A5. Rename old profiles as verification suites, not product versions.

## Phase B — complete kernel mechanisms

- [x] B1. Capability derivation and revoke.
- [ ] B2. Complete IPC, reply objects, transfer, timeout, cancellation.
- [-] B3. Fault IPC and a two-client pager protocol exist; failure/death/concurrency policies remain open.
- [-] B4. Allocator-backed frames/page tables and bounded reverse mappings exist; full root delegation and pressure evidence remain open.
- [ ] B5. Production RT scheduler and scheduling-context donation.

## Phase C — build the userspace OS

- [ ] C1. Root resource manager.
- [-] C2. Independent memory-server/pager test service exists; production service API and policies remain open.
- [ ] C3. General earlyfs ELF/process loader is the next implementation milestone.
- [ ] C4. Device/IRQ manager and console server.
- [ ] C5. Domain manager/VMM and supervisor.

## Phase D — complete real hypervisor execution

- [ ] D1. Real secondary guest CPU entry.
- [ ] D2. Four simultaneous EL2 guest execution loops.
- [ ] D3. Production virtual GIC and timer.
- [ ] D4. Real preemption and migration.
- [ ] D5. Concurrent multi-VM execution.
- [ ] D6. Secure teardown and VMID rollover.

## Phase E — devices and production certification

- [ ] E1. MMIO delegation and emulation.
- [ ] E2. SMMU and DMA isolation.
- [ ] E3. Direct device assignment and revoke.
- [ ] E4. Security hardening.
- [ ] E5. Fault injection, fuzz, and soak.
- [ ] E6. Real hardware certification and 1.0 release review.

---

# 15A. Current reconciliation notes

The prior codebase review was performed against an earlier baseline and remains valuable for architectural direction. Since that review, the tree added configuration-guarded self-tests, allocator-backed memory objects, generation-safe dynamic IPC objects, runtime process bundles, fault IPC, and independently linked pager services. These advances change several items from NOT STARTED to IN PROGRESS or COMPLETE, but they do **not** close the major production gates.

In particular:

- Hypervisor Profiles 0.3–0.6 remain verification/model suites unless a test explicitly enters independent guest instruction streams through EL2 on separate physical CPUs. Their labels must not imply production profile completion.
- The memory server and pager are real PL3 binaries and exercise real faults, but they are still certification services rather than the complete management-domain memory subsystem.
- Dynamic kernel object pools are a valid bounded implementation foundation, but they do not satisfy the final requirement for complete physical-resource discovery, delegation, scalable accounting, and pressure handling.
- AMD64 remains compile-only.
- The native ABI must not be frozen until test-only operations are removed from the production ABI surface and capability/IPC semantics are complete.

# 16. Progress summary

The following are **implemented foundations**, not full production completion:

- ARM64/QEMU boot, MMU, exceptions, GIC, timer, and SMP bring-up;
- PL3 root-task execution;
- basic kernel object and capability mechanisms;
- basic map/unmap and W^X checks;
- basic IPC/notification paths;
- real single-vCPU EL2/EL1/EL0 guest execution;
- stage-2 translation and guest stage-1 W^X;
- bounded lifecycle, interrupt, migration, and multi-VM verification models;
- deterministic acceptance and fuzz infrastructure.

Until the release gates above pass, the correct status is:

> **Zilch is an advanced production-development baseline, not yet a production-ready kernel or hypervisor.**

## Evidence update — batch 0077

- **USR-013 remains `[-] IN PROGRESS`:** a real ARM64 ELF64 `PT_LOAD` parser and
  loader now executes the three independent bootstrap programs, but selection
  is still through an embedded role registry rather than an earlyfs pathname.
- **USR-014 remains `[-] IN PROGRESS`:** bounds, overlap, alignment, executable
  entry, and W^X are enforced for the bounded bootstrap loader; full process
  policy and retained negative-test evidence remain open.
- **USR-015 remains `[ ] NOT STARTED` for production completion:** the current
  fixed stack entry remains; TLS, argv/envp, and auxiliary-vector construction
  are not implemented.
- **TST-012 remains `[-] IN PROGRESS`:** the two-client pager integration now
  additionally exercises independent ELF loading and BSS zero-fill, but full
  production pager policy and stress gates remain open.

<!-- 0121 evidence: `thread_exit` can atomically publish a supervisor badge and
terminate/deschedule the caller. This advances IPC-006 and USR-017 but does not
complete process wait/status or supervision semantics. -->

<!-- 0122 evidence: IPC reply/cancel/timeout/exit/teardown ownership changes are
serialized by an IRQ-safe lifecycle protocol. This advances IPC-003, IPC-005
through IPC-007, SEC-017, and TST-018 without claiming scalable locking or
exhaustive race-fuzz completion. -->

<!-- 0123 evidence: IPC capability minting is serialized with revoke/delete and
mapping authority transactions. `capability_transfer_revoke_race` runs the
sender and receiver on separate CPUs and verifies that no receiver descendant
exists after revoke returns, independent of which operation linearizes first.
This completes CAP-021 and advances CAP-014 and SEC-018. -->
