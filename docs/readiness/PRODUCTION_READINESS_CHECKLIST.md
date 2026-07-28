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
- [-] **PRD-006** Add CI jobs for both production and self-test configurations. Local build gates exist; hosted CI evidence is not yet retained.

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
- [-] **CAP-007** Rights attenuation enforced during derivation. Bounded implementation exists; concurrency proof remains open.

## 2.2 Derivation and revocation

- [-] **CAP-008** Bounded capability derivation tree implemented. Parent/child records and recursive traversal exist; storage is fixed-size and revoke is not yet scalable or restartable.
- [-] **CAP-009** Copy operation implemented with parent tracking. Runtime derivation/revoke/reuse cycles pass; concurrent revoke races remain open.
- [x] **CAP-010** Mint creates a rights-attenuated, badged derivation; delivery, wrong-right rejection, post-accept deletion semantics, and generation-tagged per-task endpoint badges are certified.
- [-] **CAP-011** Move operation uses locked source/destination mutation. Dedicated concurrent move/lookup evidence remains open.
- [-] **CAP-012** Single-capability delete, atomic lookup snapshots, and exception-scoped lookup/use quiescence exist; generalized non-exception readers and long-duration interleaving evidence remain open.
- [-] **CAP-013** Recursive descendant revoke uses a two-phase mark/remove pass across registered CSpaces, so children and grandchildren are removed against one intact derivation snapshot. It remains bounded, globally scanned, and not restartable.
- [-] **CAP-014** Public capability mutation/revoke APIs acquire the authority lock by construction; control, IPC transfer, and map/unmap use explicit locked transaction primitives. Scalable locking and complete concurrent mutation stress remain open.
- [-] **CAP-015** Frame destruction waits for mapping quiescence, capability revoke removes descendant mappings, and object unregister waits for pre-existing remote exception readers; per-object scalable reclamation remains open.
- [-] **CAP-016** Generation checks, reusable derivation records, and an object-table read-side grace period prevent bounded stale-reference reuse. Long-duration ABA evidence remains open.

## 2.3 Capability transfer

- [-] **CAP-017** Single-capability IPC transfer implemented for queued and direct rendezvous paths. Multi-capability transfer remains open.
- [-] **CAP-018** Receiver-selected destination slots are supported for single-capability memory-server replies; general receive windows and multi-capability placement remain open.
- [-] **CAP-019** Direct-call and reply-transfer failures preserve IPC authority and support transactional server rollback. Multi-capability partial-failure rollback and systematic fault injection remain open.
- [x] **CAP-020** A 4,096-operation cross-CSpace copy/lookup/delete fuzz sequence passes across guarded bitmap-allocated slots, including wrong-guard negatives and post-revoke reuse.
- [x] **CAP-021** Cross-CPU revoke-versus-transfer race passes with a post-revoke no-descendant invariant for both legal linearizations.

### Capability completion gate

- [ ] **CAP-GATE** Capability system is production-complete only when CAP-001 through CAP-021 pass with race, revoke, and reuse evidence.

---

# 3. IPC and fault delivery

## 3.1 Core IPC semantics

- [x] **IPC-001** Basic send/receive path exists. Runtime IPC paths pass.
- [x] **IPC-002** Synchronous call implemented.
- [-] **IPC-003** One-shot reply authority is serialized against cancellation, timeout, server exit, and teardown; scalable locking and race stress remain open.
- [-] **IPC-004** Reply-receive and reply-only operations exist; complete atomicity/race evidence remains open.
- [-] **IPC-005** Endpoint cancellation removes blocked senders/receivers with state-appropriate endpoint authority and serializes completion; forced interleaving fuzz remains open.
- [-] **IPC-006** Teardown cancellation serializes blocked state and reply authority, and blocked-destroy endpoint reuse passes; broader quiescence proof remains open.
- [-] **IPC-007** Bounded IPC timeout expiration returns deterministic error-only completions, removes write-only callers, and never spins in IRQ context. Complete timeout ABI and race stress remain open.
- [-] **IPC-008** Basic notification signal/wait and dynamic lifecycle exist; full binding policy remains open.
- [-] **IPC-009** Single-capability transfer works on calls and replies, including receiver-selected destinations for the memory-server protocol; multi-capability atomic transfer remains open.
- [ ] **IPC-010** Bounded out-of-line message strategy implemented.

## 3.2 Scheduling integration

- [x] **IPC-011** Synchronous IPC donates the caller's remaining scheduling-context budget and inherited priority to the server, including nested calls.
- [-] **IPC-012** Priority donation/inheritance propagates through a certified two-hop chain and is bounded at depth eight; measured RT inversion evidence remains open.
- [-] **IPC-013** Reply, timeout, cancellation, server exit, and teardown return unused donated budget and restore base priority; long-duration race verification remains open.
- [-] **IPC-014** IPC wakeup targets the receiver CPU where available; teardown still uses broader reschedule signaling and full targeted-IPI evidence remains open.
- [ ] **IPC-015** IPC fast-path instruction count measured.
- [ ] **IPC-016** IPC latency limits defined and met.

## 3.3 Fault IPC

- [x] **IPC-017** User page faults delivered to configured pager. Two independent clients pass.
- [x] **IPC-018** A real ARM64 PL3 `udf` exception is classified as an instruction fault, delivers syndrome and faulting PC through production fault IPC, and is contained by userspace pager policy.
- [x] **IPC-019** ABI v1 freezes a four-word fault message containing kind, architecture syndrome, fault address, and instruction pointer; layout and values are compile-time checked and validated by real PL3 data and instruction faults.
- [x] **IPC-020** Pager resume maps and restarts recoverable data faults; terminate policy kills an undefined-instruction client without disrupting the pager or other service processes.
- [x] **IPC-021** Pager exit with live reply authority immediately terminates its faulting caller; queued or accepted orphaned faults retain a kernel safety deadline and terminate deterministically on expiry.
- [x] **IPC-022** A thread may own only one pending fault record; attempted nested delivery is rejected and the faulting thread is contained instead of overwriting pager authority.

### IPC completion gate

- [ ] **IPC-GATE** IPC is production-complete only when call/reply, capability transfer, donation, timeout, cancellation, and fault IPC are all integrated and stress-tested.

---

# 4. Physical memory and address spaces

## 4.1 Boot-time memory discovery

- [-] **MEM-001** ARM64 imports bounded RAM ranges from DTB memory nodes; additional firmware formats and real-hardware evidence remain open.
- [-] **MEM-002** Kernel image, DTB blob, FDT reservation-map entries, and `/reserved-memory` ranges are excluded before allocator publication; full platform-reservation coverage remains open.
- [-] **MEM-003** DTB bounds, cell geometry, overflow, tuple shape, and post-subtraction region overlap are validated; broader malformed-map fuzz remains open.
- [-] **MEM-004** The bounded physical allocator supports up to sixteen discontiguous allocatable regions; scalable metadata and real discontiguous-hardware evidence remain open.
- [-] **MEM-005** Bootinfo v2 exports allocatable regions and root now receives an explicit memory-resource capability; scalable region-by-region delegation remains open.

## 4.2 Resource objects

- [-] **MEM-006** Allocator-backed frame/page-table pools are charged through memory-resource capabilities with explicit physical extents; extent storage now uses a shared 256-node reusable metadata pool, while unbounded/scalable allocation remains open.
- [-] **MEM-007** Frame allocation is capability-authorized and constrained to the exact physical extents owned by the selected memory resource; deterministic sorted extent traversal exists, while a scalable indexed allocator remains open.
- [-] **MEM-008** Page-table allocation uses the same delegated physical extents and accounting; scalable page-table hierarchy and metadata remain open.
- [-] **MEM-009** Memory-resource objects split and transfer physical extent nodes during delegation and retype owned pages into frame/page-table objects; rollback and coalescing exist, while restartable and unbounded retyping remain open.
- [-] **MEM-010** Per-resource and per-task quotas/accounting are enforced together with extent ownership; certification now proves fragmented return, deterministic coalescing, metadata reuse, nested exhaustion, and balanced extent return, while policy remains basic.
- [x] **MEM-011** Zero memory before delegation and reuse.
- [-] **MEM-012** Generation, owner, extent, bitmap, double-release, and overlapping-delegation checks exist; exhaustive concurrent fault injection remains open.

## 4.3 Mapping database

- [x] **MEM-013** Basic map/unmap and W^X checks exist.
- [-] **MEM-014** Up to eight mappings per frame are supported with serialized transactions; scalable representation remains open.
- [-] **MEM-015** Per-frame reverse mappings use generation-checked address-space references, and address-space teardown removes records for the exact object generation; scalable indexing remains open.
- [-] **MEM-016** Unmap by frame/address-space, frame-wide teardown, and capability-delete/revoke-driven unmapping exist with serialized authority transactions; scalable indexing and controlled race evidence remain open.
- [-] **MEM-017** Normal and device mappings validate explicit cacheability class and shareability; broader architecture/platform combinations remain open.
- [-] **MEM-018** Root-authorized allowlisted MMIO frames use device attributes and reject executable mappings; general device-resource delegation remains open.
- [-] **MEM-019** Transactional process teardown switches CPUs to the permanent kernel root and removes all tracked frame mappings before clearing user page tables; stress and scalable mapping-database evidence remain open.
- [x] **MEM-020** SMP TLB shootdown implemented and runtime verified on four CPUs.
- [x] **MEM-021** Generation-tagged ASID allocation performs global stage-1 invalidation on rollover, lazily refreshes stale live address spaces, and ignores stale-generation releases; certification rolls over before real PL3 execution.

## 4.4 User pager integration

- [-] **MEM-022** Pager endpoint is configured for test address spaces; general per-region policy remains open.
- [x] **MEM-023** Fault IPC carries fault address, access syndrome/type data, and PC.
- [x] **MEM-024** Pager map/resume is bound to the recorded fault page and access type; wrong-page and insufficient-permission replies are rejected without consuming reply authority, corrected retry succeeds, and terminate policy is runtime verified.
- [-] **MEM-025** Fault map/reply is serialized by the IPC lifecycle and mapping locks, and an already-installed identical mapping completes idempotently with one mapping record; a forced simultaneous multi-CPU interleaving remains open.
- [-] **MEM-026** The kernel enforces a bounded orphaned-fault safety deadline and pager exit consumes live fault reply authority; userspace supervisor restart/reassignment policy remains open.

### Memory completion gate

- [ ] **MEM-GATE** Memory management is production-complete only when all allocatable RAM is dynamically delegated, mapped, revoked, faulted, and reused safely without static fixture dependence.

---

# 5. Scheduler and real-time behavior

## 5.1 Scheduler core

- [x] **SCH-001** Basic SMP runnable scheduling exists and root-created workers run on CPUs 1–3.
- [-] **SCH-002** Bounded per-CPU priority selection exists; production scalability and latency evidence remain open.
- [x] **SCH-003** Deterministic priority ordering implemented.
- [-] **SCH-004** CPU pinning/affinity exists; general migration policy is deferred/open.
- [-] **SCH-005** Cross-CPU reschedule IPI exists; targeted preemption semantics and latency evidence remain open.
- [x] **SCH-006** CPU hotplug/offline is explicitly unsupported for 1.0; the online CPU set is immutable after boot.

## 5.2 Scheduling contexts

- [x] **SCH-007** Scheduling-context objects exist.
- [-] **SCH-008** Bounded budget charging/throttling and validated quiescent reconfiguration exist; RT stress evidence remains open.
- [-] **SCH-009** Bounded periodic replenishment exists; configuration rejects zero budget, zero period, budget greater than period, and deadlines that overflow the logical timebase; sporadic-server conformance remains open.
- [ ] **SCH-010** Sporadic-server semantics documented and tested.
- [x] **SCH-011** Scheduling-context budget and effective priority donation are integrated with synchronous IPC and deterministic unwind.
- [x] **SCH-012** Donation chains propagate budget and inherited priority and reject depth beyond eight.
- [x] **SCH-013** A lower-priority server executes at the caller's inherited priority until reply, cancellation, timeout, exit, or teardown.
- [x] **SCH-014** Per-CPU absolute-deadline timeout queues replace whole-thread scans; entries are generation checked and timer expiry never spins on the IPC lifecycle lock.

## 5.3 RT correctness

- [x] **SCH-015** Every active blocking kernel spinlock has a documented global rank; equal-rank CSpace locks use increasing address order and releases are strict LIFO.
- [x] **SCH-016** Generation-safe lock-order instrumentation records and reports the maximum hold duration in architectural timer ticks.
- [ ] **SCH-017** IRQ-disabled sections measured and bounded.
- [ ] **SCH-018** Logging has RT-safe deferred path.
- [x] **SCH-019** Active CPUs retain a one-tick scheduling quantum while idle CPUs program the next timeout deadline or a bounded one-second housekeeping deadline.
- [ ] **SCH-020** Interrupt latency target defined and met.
- [ ] **SCH-021** Preemption latency target defined and met.
- [ ] **SCH-022** Cross-CPU wake latency target defined and met.
- [ ] **SCH-023** IPC latency target defined and met.
- [ ] **SCH-024** Multi-hour RT stress test passes without deadline violation.

### Scheduler completion gate

- [ ] **SCH-GATE** Scheduler is production-complete only when budget, replenishment, donation, priority inheritance, migration, and measured RT latency limits all pass.

---

# 6. Interrupts, timers, and platform support

## 6.1 ARM64 interrupt subsystem

- [x] **IRQ-001** GICv3 distributor and CPU interfaces initialize on QEMU ARM64 virt.
- [-] **IRQ-002** An exclusive IRQ registry binds one generation-checked interrupt object to each GIC line; data-driven external IRQ discovery/publication remains open.
- [-] **IRQ-003** Registered IRQ capabilities support rights-attenuated cross-CSpace delegation and revoke; a real userspace device-manager delegation flow remains open.
- [x] **IRQ-004** GIC mask/unmask, priority-drop, explicit deactivate, active-state validation, and notification-gated acknowledge semantics are implemented.
- [-] **IRQ-005** Edge configuration and the level-triggered timer path pass QEMU integration; real external edge/level device evidence remains open.
- [x] **IRQ-006** IRQ ownership is exclusive for 1.0: a second object cannot register the same physical line; shared-line demultiplexing is delegated to a userspace driver service.
- [x] **IRQ-007** A bounded delivery window masks a line after 64 events and requires explicit rebinding/recovery before delivery resumes.
- [x] **IRQ-008** Per-IRQ delivered, acknowledged, suppressed, window-count, active, masked, and stormed diagnostics are maintained.

## 6.2 Timers

- [x] **TIM-001** Architectural virtual timer initializes and per-CPU progress is verified.
- [x] **TIM-002** Each ARM64 CPU programs its local virtual timer from an absolute scheduler deadline.
- [x] **TIM-003** The head of each per-CPU timeout queue drives idle timer programming without losing elapsed logical ticks.
- [x] **TIM-004** Counter frequency, hardware interval bounds, zero-delay behavior, and deadline-addition overflow are validated and fail closed.
- [x] **TIM-005** Suspend/resume is explicitly out of scope for 1.0; timer and scheduler state assume one uninterrupted boot.

## 6.3 Platform support

- [x] **PLT-001** QEMU ARM64 virt supported for the current development/certification profile.
- [ ] **PLT-002** At least one real ARM64 hardware platform supported.
- [ ] **PLT-003** Platform description is data-driven, not hard-coded.
- [ ] **PLT-004** UART ownership transitions from early kernel console to userspace driver.
- [ ] **PLT-005** Watchdog supported.
- [ ] **PLT-006** Reset and power-off supported.

## 6.4 AMD64 truthfulness

- [x] **PLT-007** Mark AMD64 compile-only until runtime backend exists.
- [ ] **PLT-008** Implement IDT and exception entry.
- [ ] **PLT-009** Implement APIC and interrupt routing.
- [ ] **PLT-010** Implement SMP startup.
- [ ] **PLT-011** Implement page-table backend.
- [ ] **PLT-012** Implement timer backend.
- [x] **PLT-013** AMD64 virtualization is explicitly deferred beyond 1.0 together with the compile-only AMD64 runtime.

---

# 7. Userspace control-plane OS

## 7.1 Root resource manager

- [x] **USR-001** PL3 root task boots.
- [-] **USR-002** Root receives memory inventory metadata and existing bootstrap capabilities; explicit capability delegation for all allocatable RAM remains open.
- [ ] **USR-003** Root task contains no kernel acceptance-test policy in production.
- [ ] **USR-004** Root task launches and supervises core servers.
- [ ] **USR-005** Root task exposes resource-allocation policy through documented IPC.

## 7.2 Memory server and pager

- [-] **USR-006** The independently linked PL3 memory server now allocates frames through its delegated memory-resource capability; production inventory/policy APIs remain open.
- [-] **USR-007** Root bootinfo carries the physical memory inventory; the userspace memory server does not yet import and manage it.
- [-] **USR-008** The PL3 memory server provides resource-backed allocation and transfers derived frame capabilities into client-selected slots; general libraries, asynchronous queues, and scalable handle management remain open.
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

- [ ] **USR-025** Userspace domain manager/VMM implemented.
- [ ] **USR-026** VM creation uses capability-authorized kernel APIs.
- [ ] **USR-027** Guest image loading performed in userspace.
- [ ] **USR-028** VM memory and device assignment policy remains in userspace.
- [ ] **USR-029** VM lifecycle exposed through stable management API.
- [ ] **USR-030** Linux guest launch demonstrated.
- [ ] **USR-031** BSD guest launch demonstrated.
- [ ] **USR-032** Zephyr guest launch demonstrated.

## 7.6 Supervision

- [ ] **USR-033** Service supervisor implemented.
- [ ] **USR-034** Restart policies implemented.
- [ ] **USR-035** Dependency ordering implemented.
- [ ] **USR-036** Crash-loop containment implemented.
- [ ] **USR-037** Structured health reporting implemented.

### Userspace completion gate

- [ ] **USR-GATE** The control-plane OS is complete only when the kernel boots a real management-domain service graph and all core policies execute in userspace.

---

# 8. Hypervisor core

## 8.1 VM and vCPU object model

- [-] **HYP-001** Capability-authorized VM/vCPU objects exist; full lifecycle and userspace VMM integration remain open.
- [-] **HYP-002** Basic VMID allocation and stage-2 roots exist.
- [ ] **HYP-003** Full lifecycle: create/configure/load/start/pause/resume/reset/stop/destroy.
- [x] **HYP-004** Per-VM accounting tracks current/peak mapped pages, map/unmap totals, active vCPUs, and run entry/exit balance with overflow/underflow fault detection.
- [ ] **HYP-005** Complete vCPU architectural state definition.
- [ ] **HYP-006** Reserved and unsupported guest state sanitized.
- [ ] **HYP-007** VM teardown is race-safe under concurrent execution.

## 8.2 Stage-2 translation

- [-] **HYP-008** Basic stage-2 map/unmap and W^X exist.
- [ ] **HYP-009** Dynamic stage-2 table allocation implemented.
- [ ] **HYP-010** Complete memory attribute validation implemented.
- [ ] **HYP-011** Dirty/access tracking strategy implemented if required.
- [ ] **HYP-012** Stage-2 fault delivered to userspace VMM when policy is needed.
- [x] **HYP-013** Generation-tagged VMID allocation performs global stage-2 invalidation on rollover, lazily refreshes stale live VMs before mapping/reset/run, and ignores stale-generation releases.
- [ ] **HYP-014** Concurrent map/unmap versus vCPU execution tested.
- [ ] **HYP-015** No guest mapping can target kernel, hypervisor, or another VM memory.

## 8.3 Real multi-vCPU execution

- [x] **HYP-016** Real single-vCPU EL2/EL1/EL0 execution works.
- [ ] **HYP-017** Secondary guest CPU entry code executes through EL2.
- [ ] **HYP-018** Four physical CPUs concurrently run four guest vCPUs.
- [ ] **HYP-019** Guest-side SMP barrier completes using real guest instructions.
- [ ] **HYP-020** Full vCPU state saved on preemption.
- [ ] **HYP-021** vCPU resumed on another physical CPU.
- [ ] **HYP-022** VMID/ASID/TLB maintenance verified during migration.
- [ ] **HYP-023** Two VMs execute concurrently through EL2.
- [ ] **HYP-024** One guest crash does not stop or corrupt another VM.

## 8.4 Virtual interrupt controller

- [-] **HYP-025** Bounded software interrupt model exists; it is not production virtual GIC completion.
- [ ] **HYP-026** Production virtual GIC architecture implemented.
- [ ] **HYP-027** SGI, PPI, and SPI semantics implemented.
- [ ] **HYP-028** Priority and masking semantics implemented.
- [ ] **HYP-029** Level and edge semantics implemented.
- [ ] **HYP-030** Pending/active/deactivate lifecycle implemented.
- [ ] **HYP-031** Maintenance interrupt handling implemented.
- [ ] **HYP-032** GIC virtualization hardware used where available.
- [ ] **HYP-033** Software fallback provides equivalent observable semantics.

## 8.5 Virtual timers

- [-] **HYP-034** Basic virtual timer event injection works in the current verification path.
- [ ] **HYP-035** `CNTV_CTL_EL0` state saved/restored.
- [ ] **HYP-036** `CNTV_CVAL_EL0` state saved/restored.
- [ ] **HYP-037** Virtual counter offset managed per VM.
- [ ] **HYP-038** Timer expiry while vCPU is descheduled handled correctly.
- [ ] **HYP-039** Timer state remains correct across migration.
- [ ] **HYP-040** Timer races and cancellation stress-tested.

## 8.6 Hypercalls and exits

- [-] **HYP-041** Basic test hypercalls work; stable production ABI remains open.
- [ ] **HYP-042** Stable userspace-visible hypercall ABI defined if needed.
- [x] **HYP-043** Unknown guest hypercalls exit to the host as bounded `hypercall` exits with the rejected call number preserved in the qualification field.
- [ ] **HYP-044** MMIO exits delivered to userspace VMM.
- [ ] **HYP-045** WFI/WFE behavior correctly virtualized.
- [-] **HYP-046** Trapped guest SCTLR_EL1 writes are masked to supported controls with mandatory RES1 bits restored; complete trapped system-register coverage remains open.
- [x] **HYP-047** Guest abort exits record ESR, FAR, guest PC, and reconstructed IPA from HPFAR/FAR; negative certification validates abort classification and IPA reconstruction.

### Hypervisor execution gate

- [ ] **HYP-EXEC-GATE** Real multi-vCPU execution is complete only when four guest instruction streams execute concurrently through EL2 on four physical CPUs and pass guest-side SMP/IPI/timer tests.

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

- [-] **SEC-007** Kernel MAIR/TCR programming uses constructed constants and trapped guest SCTLR_EL1 values are sanitized; a complete register-by-register reserved-bit audit remains open.
- [-] **SEC-008** Every CPU inventories CSV2/CSV3/SSBS/PAuth/BTI and validation boundaries execute architectural CSDB+ISB; real-platform hardware/firmware mitigation qualification remains open.
- [x] **SEC-009** Pointer authentication was evaluated and is explicitly deferred until all C++ and hand-written exception/boot/guest-entry paths can be signed and negatively tested together.
- [x] **SEC-010** BTI was evaluated and is explicitly deferred until every indirect target, vector, context-switch, and guest-entry assembly path has audited landing pads.
- [x] **SEC-011** PAN is enabled and UAO disabled on CPUs advertising each extension, with bootstrap readback verification; unsupported baseline Armv8-A CPUs safely skip optional instructions.
- [x] **SEC-012** Release kernels exclude IPC fuzz/debug decoding, deny the guest diagnostic hypercall, omit detailed EL2 console walks, and retain only bounded production diagnostics.

## 10.3 Concurrency hardening

- [x] **SEC-013** Endpoint, IPC lifecycle, capability, mapping, allocator, and object-table locks follow the documented global hierarchy.
- [x] **SEC-014** Certification builds check per-CPU acquisition rank, recursion, equal-rank address order, depth, and reverse release; the full four-CPU suite reports zero violations.
- [-] **SEC-015** Object and VM lifecycle counters reject overflow/underflow and expose accounting faults; remaining reference-bearing subsystems require the same checked-counter audit.
- [-] **SEC-016** ABA hazards are bounded by generation-tagged references, per-CPU thread bindings, and object-table read-side grace periods before reuse; long-duration wraparound evidence remains open.
- [-] **SEC-017** User-thread teardown has generation-tagged return-frame quiescence and switches CPUs to the permanent kernel TTBR0 root before reclaiming user page tables; IRQ, VM/vCPU, and remaining object teardown protocols are open.
- [-] **SEC-018** Race tests cover capability revoke versus IPC transfer, object lookup/use versus destroy, IPC lifecycle versus destroy/timeout/cancel, and vCPU execution; controlled map/IRQ races and broader stress remain open.

## 10.4 Failure handling

- [x] **SEC-019** Fatal exception and stack-corruption handling masks all exception classes and records through lock-free emergency storage without consulting scheduler, allocator, capability, object, or console-lock state; certification poisons scheduler identity and holds printk locked while validating capture.
- [x] **SEC-020** Each CPU has a lock-free 32-record emergency ring for exception entry, fatal traps, stack corruption, and bounded-printk contention.
- [x] **SEC-021** Fatal exceptions preserve a checksummed EL/vector/ESR/FAR/PC crash record in a linker-reserved `.noinit` page excluded from BSS clearing.
- [ ] **SEC-022** Watchdog integration implemented.
- [x] **SEC-023** Recoverable user instruction/data faults are delivered through fault IPC or isolate only the faulting thread; pager recovery and continued four-CPU acceptance prove the kernel remains live.
- [x] **SEC-024** Guest traps always return through bounded VM exits; unexpected traps fault only the owning vCPU/VM, while stage-2 faults remain recoverable VMM exits.

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

---

# 12. Testing and verification

## 12.1 Host testing

- [ ] **TST-001** Architecture-independent kernel logic builds as host tests.
- [ ] **TST-002** Capability unit tests implemented.
- [ ] **TST-003** IPC state-machine unit tests implemented.
- [ ] **TST-004** Scheduler unit tests implemented.
- [ ] **TST-005** VM lifecycle unit tests implemented.
- [ ] **TST-006** Stage-2 table unit tests implemented.
- [ ] **TST-007** Property-based tests implemented for bounded models.
- [ ] **TST-008** Sanitizer jobs run against portable components.

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
- [ ] **TST-017** Capability derivation/revoke fuzz implemented.
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

- [-] **TST-031** Clang static analyzer clean or deviations documented; the required `scan-build` tool is not installed in the current certification environment, and `static-analysis-tools-check` records the release-blocking deviation.
- [-] **TST-032** clang-tidy safety profile clean or deviations documented; the required `clang-tidy` tool is not installed in the current certification environment, and `static-analysis-tools-check` records the release-blocking deviation.
- [ ] **TST-033** Undefined-behavior checks run on portable code.
- [ ] **TST-034** Stack usage measured and bounded.
- [x] **TST-035** Release ELF section flags are audited for W+X sections, executable text, and non-writable rodata on both supported build profiles.
- [x] **TST-036** Reproducible release builds are verified byte-for-byte for ARM64 and AMD64 ELF, raw image, userspace ELF/map, and early filesystem artifacts with a fixed source epoch; map paths are normalized before comparison.
- [ ] **TST-037** Coverage report generated and reviewed.

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

- [ ] Product/test separation gate
- [ ] Capability completion gate
- [ ] IPC completion gate
- [ ] Memory completion gate
- [ ] Scheduler completion gate
- [ ] Interrupt and timer production gate
- [ ] Userspace control-plane gate
- [ ] Security and hardening gate
- [ ] Verification and soak gate
- [ ] Documentation and conformance gate
- [ ] Real hardware ARM64 certification gate

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

- [ ] B1. Capability derivation and revoke.
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
