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
- [-] **PRD-003** Exclude all self-test code from production builds. Test dispatch and the embedded guest fixture are excluded; hypervisor model implementation still needs extraction from production headers.
- [x] **PRD-004** Ensure production kernel boots with all self-test options disabled. Runtime evidence: release boot reports `selftests=disabled`.
- [x] **PRD-005** Ensure production binary contains no profile-specific guest images or test fixtures. Evidence: release ELF symbol/string gate in batch 0079.
- [-] **PRD-006** Add CI jobs for both production and self-test configurations. Local build gates exist; hosted CI evidence is not yet retained.

## 1.2 ABI cleanup

- [x] **PRD-007** Remove acceptance-report operations from the production ABI. Evidence: separate `sys::test_abi` in batch 0079.
- [x] **PRD-008** Remove worker-tick and other certification operations from the production ABI. Product process/object operations remain product ABI mechanisms.
- [x] **PRD-009** Move hypervisor self-test entry points behind test configuration. Evidence: separate certification ABI and `CONFIG_HYPERVISOR_SELFTEST` guest-object gating in batch 0079.
- [-] **PRD-010** Version the native `sys` ABI. Headers use `sys/v1`, but compatibility/deprecation guarantees are not frozen.
- [ ] **PRD-011** Document compatibility and deprecation rules.
- [ ] **PRD-012** Add ABI layout checks for all public structures.

## 1.3 Module boundaries

- [ ] **PRD-013** Split hypervisor object model from ARM64 EL2 backend.
- [ ] **PRD-014** Split stage-2 translation management into its own module.
- [ ] **PRD-015** Split virtual interrupt and timer state into dedicated modules.
- [ ] **PRD-016** Split VM lifecycle and VMID allocation into dedicated modules.
- [ ] **PRD-017** Move profile tests into test-only modules.
- [ ] **PRD-018** Establish maximum source/header size and dependency rules.

---

# 2. Kernel capability system

## 2.1 Capability representation

- [x] **CAP-001** Generation-safe object references exist. Runtime destroy/reuse evidence exists.
- [x] **CAP-002** Basic rights checks exist. Rights attenuation and negative checks are exercised.
- [ ] **CAP-003** Hierarchical CSpace design completed.
- [ ] **CAP-004** Guarded capability lookup implemented.
- [ ] **CAP-005** Scalable slot allocation implemented.
- [-] **CAP-006** Capability badges implemented. Basic badges/minting exist; scalable guarded CSpace semantics remain open.
- [-] **CAP-007** Rights attenuation enforced during derivation. Bounded implementation exists; concurrency proof remains open.

## 2.2 Derivation and revocation

- [-] **CAP-008** Bounded capability derivation tree implemented. Parent/child records and recursive traversal exist; storage is fixed-size and revoke is not yet scalable or restartable.
- [-] **CAP-009** Copy operation implemented with parent tracking. Runtime derivation/revoke/reuse cycles pass; concurrent revoke races remain open.
- [-] **CAP-010** Mint operation implemented with reduced rights and badge. Rights escalation is rejected; guarded CSpace and complete badge-delivery semantics remain open.
- [-] **CAP-011** Move operation uses locked source/destination mutation. Dedicated concurrent move/lookup evidence remains open.
- [-] **CAP-012** Single-capability delete exists. Concurrent lookup and teardown-race evidence remains open.
- [-] **CAP-013** Recursive descendant revoke uses a two-phase mark/remove pass across registered CSpaces, so children and grandchildren are removed against one intact derivation snapshot. It remains bounded, globally scanned, and not restartable.
- [ ] **CAP-014** Revocation is safe under concurrent lookup and IPC.
- [ ] **CAP-015** Object destruction waits for capability/reference quiescence.
- [-] **CAP-016** Generation checks and reusable derivation records prevent bounded stale-reference reuse. Long-duration and concurrent ABA evidence remains open.

## 2.3 Capability transfer

- [-] **CAP-017** Single-capability IPC transfer implemented for queued and direct rendezvous paths. Multi-capability transfer remains open.
- [ ] **CAP-018** Receiver-controlled destination slots implemented.
- [-] **CAP-019** Direct-transfer failure restores the waiting receiver and avoids caller blocking. Multi-capability partial-failure rollback and fault injection remain open.
- [ ] **CAP-020** Cross-CSpace transfer fuzz test passes.
- [ ] **CAP-021** Concurrent revoke-versus-transfer race test passes.

### Capability completion gate

- [ ] **CAP-GATE** Capability system is production-complete only when CAP-001 through CAP-021 pass with race, revoke, and reuse evidence.

---

# 3. IPC and fault delivery

## 3.1 Core IPC semantics

- [x] **IPC-001** Basic send/receive path exists. Runtime IPC paths pass.
- [x] **IPC-002** Synchronous call implemented.
- [-] **IPC-003** One-shot reply authority implemented. Full production semantics and race stress remain open.
- [-] **IPC-004** Reply-receive and reply-only operations exist; complete atomicity/race evidence remains open.
- [-] **IPC-005** Bounded endpoint cancellation exists; forced race certification remains open.
- [-] **IPC-006** Teardown cancellation exists; concurrency/quiescence proof remains open.
- [-] **IPC-007** Bounded IPC timeout expiration exists. Complete timeout ABI, race stress, and donation rollback remain open.
- [-] **IPC-008** Basic notification signal/wait and dynamic lifecycle exist; full binding policy remains open.
- [-] **IPC-009** Single-capability transfer exists; multi-capability atomic transfer remains open.
- [ ] **IPC-010** Bounded out-of-line message strategy implemented.

## 3.2 Scheduling integration

- [ ] **IPC-011** Scheduling-context donation implemented.
- [-] **IPC-012** Bounded priority donation/inheritance exists; chain and RT evidence remain open.
- [-] **IPC-013** Bounded donation cleanup paths exist with timeout/cancel foundations; chain and race verification remain open.
- [-] **IPC-014** IPC wakeup targets the receiver CPU where available; teardown still uses broader reschedule signaling and full targeted-IPI evidence remains open.
- [ ] **IPC-015** IPC fast-path instruction count measured.
- [ ] **IPC-016** IPC latency limits defined and met.

## 3.3 Fault IPC

- [x] **IPC-017** User page faults delivered to configured pager. Two independent clients pass.
- [ ] **IPC-018** Undefined instruction faults delivered to fault handler.
- [-] **IPC-019** Fault address, access syndrome, and PC are delivered; cross-architecture metadata contract is not frozen.
- [-] **IPC-020** Resume path passes and terminate mechanism exists; negative-policy coverage remains open.
- [ ] **IPC-021** Pager death handling implemented.
- [ ] **IPC-022** Nested fault handling is bounded and deterministic.

### IPC completion gate

- [ ] **IPC-GATE** IPC is production-complete only when call/reply, capability transfer, donation, timeout, cancellation, and fault IPC are all integrated and stress-tested.

---

# 4. Physical memory and address spaces

## 4.1 Boot-time memory discovery

- [-] **MEM-001** QEMU RAM is represented as an explicit allocatable physical-region inventory; general DT/firmware memory-node parsing remains open.
- [-] **MEM-002** The kernel image is page-aligned and excluded before the allocatable region is published; malformed/general firmware-region handling remains open.
- [ ] **MEM-003** Detect overlapping and malformed regions.
- [ ] **MEM-004** Support multiple discontiguous RAM regions.
- [ ] **MEM-005** Export remaining memory to the root resource server.

## 4.2 Resource objects

- [-] **MEM-006** Allocator-backed dynamic frame/page-table pools exist, but root delegation is bounded and not yet a complete untyped resource model.
- [-] **MEM-007** Concurrent frame allocation, owner identity, zero-on-allocation/reuse, and accounting exist; full root-resource delegation remains open.
- [-] **MEM-008** Concurrent allocator-backed page-table objects carry owner identity and accounting; scalable hierarchy and delegation remain open.
- [ ] **MEM-009** Implement untyped/resource retyping or equivalent safe delegation.
- [-] **MEM-010** Bounded per-task quotas/accounting are now exercised across frame and page-table create/destroy cycles; policy and pressure exhaustion remain open.
- [x] **MEM-011** Zero memory before delegation and reuse.
- [-] **MEM-012** Basic generation/ownership checks exist; exhaustive fault injection remains open.

## 4.3 Mapping database

- [x] **MEM-013** Basic map/unmap and W^X checks exist.
- [-] **MEM-014** Up to eight mappings per frame are supported with serialized transactions; scalable representation remains open.
- [-] **MEM-015** Per-frame reverse mappings use generation-checked address-space references, and address-space teardown removes records for the exact object generation; scalable indexing remains open.
- [-] **MEM-016** Unmap by frame/address-space and frame-wide teardown exist with serialized record updates; capability-revoke integration and concurrent race evidence remain open.
- [ ] **MEM-017** Cacheability/shareability attributes validated.
- [ ] **MEM-018** Device-memory mappings use correct attributes.
- [-] **MEM-019** Transactional process teardown switches CPUs to the permanent kernel root and removes all tracked frame mappings before clearing user page tables; stress and scalable mapping-database evidence remain open.
- [x] **MEM-020** SMP TLB shootdown implemented and runtime verified on four CPUs.
- [ ] **MEM-021** ASID allocation, rollover, and reuse implemented.

## 4.4 User pager integration

- [-] **MEM-022** Pager endpoint is configured for test address spaces; general per-region policy remains open.
- [x] **MEM-023** Fault IPC carries fault address, access syndrome/type data, and PC.
- [-] **MEM-024** Map/resume and terminate mechanisms exist; denial/invalid-reply policy tests remain open.
- [ ] **MEM-025** Concurrent faults to the same page are serialized correctly.
- [ ] **MEM-026** Pager timeout/death policy implemented outside the kernel.

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
- [ ] **SCH-006** CPU hotplug/offline state handled or explicitly unsupported for 1.0.

## 5.2 Scheduling contexts

- [x] **SCH-007** Scheduling-context objects exist.
- [-] **SCH-008** Bounded budget charging/throttling exists; RT stress evidence remains open.
- [-] **SCH-009** Bounded replenishment exists; sporadic-server conformance remains open.
- [ ] **SCH-010** Sporadic-server semantics documented and tested.
- [-] **SCH-011** Priority donation is integrated; full scheduling-context budget donation remains open.
- [ ] **SCH-012** Donation chains are bounded.
- [ ] **SCH-013** Priority inheritance prevents inversion.
- [ ] **SCH-014** Timeout queue implemented.

## 5.3 RT correctness

- [ ] **SCH-015** Kernel locks have documented ordering.
- [ ] **SCH-016** Maximum lock hold times measured.
- [ ] **SCH-017** IRQ-disabled sections measured and bounded.
- [ ] **SCH-018** Logging has RT-safe deferred path.
- [ ] **SCH-019** Tickless scheduling implemented where required.
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
- [ ] **IRQ-002** Complete interrupt registration and ownership model.
- [ ] **IRQ-003** IRQ capabilities delegated to userspace.
- [ ] **IRQ-004** Mask, unmask, acknowledge, and deactivate semantics completed.
- [ ] **IRQ-005** Level and edge interrupt behavior tested.
- [ ] **IRQ-006** Shared interrupt policy explicitly defined.
- [ ] **IRQ-007** Interrupt storm containment implemented.
- [ ] **IRQ-008** Per-IRQ accounting and diagnostics implemented.

## 6.2 Timers

- [x] **TIM-001** Architectural virtual timer initializes and per-CPU progress is verified.
- [ ] **TIM-002** Per-CPU deadline programming implemented.
- [ ] **TIM-003** Tickless timeout queue integrated.
- [ ] **TIM-004** Counter frequency and overflow behavior validated.
- [ ] **TIM-005** Suspend/resume behavior implemented or explicitly out of scope.

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
- [ ] **PLT-013** Implement virtualization backend or explicitly defer it beyond 1.0.

---

# 7. Userspace control-plane OS

## 7.1 Root resource manager

- [x] **USR-001** PL3 root task boots.
- [ ] **USR-002** Root task receives all delegated resources explicitly.
- [ ] **USR-003** Root task contains no kernel acceptance-test policy in production.
- [ ] **USR-004** Root task launches and supervises core servers.
- [ ] **USR-005** Root task exposes resource-allocation policy through documented IPC.

## 7.2 Memory server and pager

- [-] **USR-006** Independently linked userspace memory-server test service exists; production resource-server API/policy remains open.
- [ ] **USR-007** Physical memory inventory imported from root resources.
- [-] **USR-008** Capability-authorized frame creation is exercised by the pager; general service API remains open.
- [-] **USR-009** Independent pager service handles two sequential clients; concurrency, death, and pressure policies remain open.
- [ ] **USR-010** Demand paging implemented where configured.
- [ ] **USR-011** Memory pressure and allocation-failure policy implemented.
- [ ] **USR-012** Per-domain quotas implemented.

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
- [ ] **HYP-004** Resource accounting per VM.
- [ ] **HYP-005** Complete vCPU architectural state definition.
- [ ] **HYP-006** Reserved and unsupported guest state sanitized.
- [ ] **HYP-007** VM teardown is race-safe under concurrent execution.

## 8.2 Stage-2 translation

- [-] **HYP-008** Basic stage-2 map/unmap and W^X exist.
- [ ] **HYP-009** Dynamic stage-2 table allocation implemented.
- [ ] **HYP-010** Complete memory attribute validation implemented.
- [ ] **HYP-011** Dirty/access tracking strategy implemented if required.
- [ ] **HYP-012** Stage-2 fault delivered to userspace VMM when policy is needed.
- [ ] **HYP-013** VMID rollover and global invalidation implemented.
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
- [ ] **HYP-043** Unknown hypercalls fail safely.
- [ ] **HYP-044** MMIO exits delivered to userspace VMM.
- [ ] **HYP-045** WFI/WFE behavior correctly virtualized.
- [ ] **HYP-046** Guest system-register traps validated and sanitized.
- [ ] **HYP-047** Guest aborts produce complete fault records.

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

- [-] **SEC-001** Kernel and guest W^X checks exist in bring-up paths.
- [ ] **SEC-002** W^X enforced in every production address space.
- [ ] **SEC-003** Kernel read-only data protected after initialization.
- [ ] **SEC-004** Kernel stacks have guard pages.
- [ ] **SEC-005** User copy routines validate full ranges and overflow.
- [ ] **SEC-006** Reused memory and architectural state are zeroed.

## 10.2 Architecture hardening

- [ ] **SEC-007** Reserved system-register bits sanitized.
- [ ] **SEC-008** Speculation mitigations evaluated and implemented.
- [ ] **SEC-009** Pointer authentication strategy evaluated.
- [ ] **SEC-010** Branch target identification strategy evaluated.
- [ ] **SEC-011** PAN/UAO or equivalent protections configured where applicable.
- [ ] **SEC-012** Debug interfaces disabled or capability-controlled in production.

## 10.3 Concurrency hardening

- [ ] **SEC-013** Lock hierarchy documented.
- [ ] **SEC-014** Debug lock-order checker implemented.
- [ ] **SEC-015** Refcount overflow/underflow prevented.
- [-] **SEC-016** ABA hazards addressed for reused objects. Generation-tagged per-CPU thread bindings prevent slot reuse from aliasing an older return-frame owner; broader object-class and long-duration race evidence remains open.
- [-] **SEC-017** User-thread teardown has generation-tagged return-frame quiescence and switches CPUs to the permanent kernel TTBR0 root before reclaiming user page tables; IRQ, VM/vCPU, and remaining object teardown protocols are open.
- [ ] **SEC-018** Race tests cover revoke, destroy, map, IPC, IRQ, and vCPU execution.

## 10.4 Failure handling

- [ ] **SEC-019** Panic path works with corrupted scheduler state.
- [ ] **SEC-020** Per-CPU emergency log buffer implemented.
- [ ] **SEC-021** Crash record preserved for postmortem analysis.
- [ ] **SEC-022** Watchdog integration implemented.
- [ ] **SEC-023** Recoverable userspace failures do not panic the kernel.
- [ ] **SEC-024** Guest failures are contained to the owning VM.

---

# 11. Observability and diagnostics

- [x] **OBS-001** Structured kernel log levels exist.
- [ ] **OBS-002** Logging is safe in IRQ and exception contexts.
- [ ] **OBS-003** RT-safe deferred logging path implemented.
- [ ] **OBS-004** Per-CPU trace buffers implemented.
- [ ] **OBS-005** IPC, scheduler, IRQ, VM-exit, and fault tracing implemented.
- [ ] **OBS-006** Trace overhead can be disabled in production.
- [ ] **OBS-007** Resource leak counters implemented.
- [ ] **OBS-008** VM lifecycle and assignment audit records implemented.
- [ ] **OBS-009** Stable diagnostic format documented.
- [ ] **OBS-010** Sensitive guest/user data excluded from logs by default.

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
- [-] **TST-018** Reply/cancel-versus-thread-teardown regressions are covered by bounded certification paths; dedicated race fuzz and fault injection remain open.
- [ ] **TST-019** Mapping/revoke/TLB-shootdown race fuzz implemented.
- [ ] **TST-020** Scheduler migration/preemption race fuzz implemented.
- [ ] **TST-021** VM lifecycle and VMID rollover fuzz implemented.
- [ ] **TST-022** Virtual interrupt storm test implemented.
- [-] **TST-023** Certification exercises bounded allocation/accounting plus multi-map, partial-unmap, destroy rejection, cleanup, and reuse; full allocator exhaustion and sustained pressure remain open.
- [ ] **TST-024** Fault injection covers every allocation and teardown stage.

## 12.4 Long-duration certification

- [ ] **TST-025** 24-hour kernel SMP soak passes.
- [ ] **TST-026** 24-hour multi-VM soak passes.
- [ ] **TST-027** 72-hour mixed workload soak passes.
- [ ] **TST-028** Repeated reboot and lifecycle test passes.
- [ ] **TST-029** No memory/resource growth across soak runs.
- [ ] **TST-030** No missed deadlines under defined RT workload.

## 12.5 Static verification

- [ ] **TST-031** Clang static analyzer clean or deviations documented.
- [ ] **TST-032** clang-tidy safety profile clean or deviations documented.
- [ ] **TST-033** Undefined-behavior checks run on portable code.
- [ ] **TST-034** Stack usage measured and bounded.
- [ ] **TST-035** Binary section permissions audited.
- [ ] **TST-036** Reproducible build verified.
- [ ] **TST-037** Coverage report generated and reviewed.

---

# 13. Documentation and architecture conformance

- [ ] **DOC-001** `arch_design.md` reflects implemented architecture.
- [ ] **DOC-002** `detail_design.md` reflects implemented mechanisms.
- [x] **DOC-003** Every mandatory requirement has a stable ID in this checklist.
- [ ] **DOC-004** Every requirement maps to implementation and tests.
- [-] **DOC-005** Model-only runtime results now use `HV-MODEL` and `hypervisor_control_model`; legacy profile documents still require complete renaming and archival.
- [ ] **DOC-006** Unsupported features are explicitly documented.
- [ ] **DOC-007** Threat model documented.
- [ ] **DOC-008** Trust boundaries documented.
- [ ] **DOC-009** Capability and IPC semantics documented formally enough for independent implementation.
- [ ] **DOC-010** Memory-ordering rules documented.
- [ ] **DOC-011** Locking and teardown protocols documented.
- [ ] **DOC-012** Hypervisor guest-visible architecture documented.
- [ ] **DOC-013** Userspace server APIs documented.
- [ ] **DOC-014** Release and compatibility policy documented.

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
