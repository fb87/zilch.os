# Zilch Production-Readiness Checklist

Status: **Authoritative project progress tracker**  
Baseline reconciled through: **patch 0130 runtime evidence and documentation state**  
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
- [x] **PRD-019** A top-level Kconfig hierarchy generates `.config`, `auto.conf`, and `autoconf.h`; generated configuration is the sole source of `CONFIG_*` values for Make, C, C++, and assembly. Evidence: root `Kconfig` sources `src/kernel/Kconfig`, `src/user/Kconfig`, and `samples/guests/Kconfig`; `mk/config.mk` drives `tools/config/generate.py` to emit all three artifacts and exports the resulting `CONFIG_*` set to the submakes.
- [-] **PRD-020** Checked-in `configs/{debug,release,guest}_defconfig` exist and select the profile through `KCONFIG_DEFCONFIG`; release makes every test/debug option unavailable (`CONFIG_TESTS`/`VERBOSE_DIAGNOSTICS`/`TRACE`/`DEBUG_INFO` all `depends on BUILD_DEBUG`). Open: the defconfigs have not yet *replaced* the variant set -- `Makefile`'s `certification`/`debug` targets still select `BUILD_VARIANT`, and `development`/`release-guest` variant trees remain live, so two selection mechanisms coexist.
- [x] **PRD-021** Release compiler flags exclude debug information and enable product optimization; release source/ELF gates reject tests, traces, debug strings, fixtures, and DWARF sections. Evidence: `CONFIG_DEBUG_INFO` is `default y if BUILD_DEBUG` only; `tools/release/check_production_elf.sh` fails on any `.debug_`/`.zdebug_` section, alongside `check_production_source.sh` and `check_section_permissions.sh`.
- [x] **PRD-022** Kconfig exposes generic built-in, external, interactive, and per-sample guest enablement without making an external guest toolchain a core build dependency. Evidence: `CONFIG_GUEST_SUPPORT`/`GUEST_TEST_ARM64`/`GUEST_EXTERNAL`/`GUEST_EMBEDDED_IMAGE`/`GUEST_INTERACTIVE` in `src/user/Kconfig`, with `samples/guests/Kconfig` adding per-sample `GUEST_ZEPHYR`/`GUEST_LINUX`/`GUEST_FREEBSD` gated on `GUEST_EXTERNAL`.
- [x] **PRD-023** Guest examples live under `samples/guests/<name>/` with sample-local pinned fetch, nested toolchain shell, build, output, and acceptance ownership; the root release build succeeds with no sample fetched. Evidence: `samples/guests/zephyr/` owns its own Kconfig, pinned fetch, and `acceptance` target; the only `samples/` reference in the root build is the checked-in `samples/guests/Kconfig` on `KCONFIG_SOURCES`, so no sample tree is a build input.

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
- [x] **PLT-004** The 1.0 virtual profile deliberately retains the polling UART as a kernel diagnostic console (kernel `console_puts`/`pr_info` write the physical PL011 directly, bypassing the capability system entirely -- unaffected by anything below). Userspace/guest access to the same physical PL011 is now separately capability-mediated and exclusively owned by the console-server (USR-022/USR-023); the guest no longer gets direct passthrough at all -- see USR-020.
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

- [-] **USR-013** Bootstrap loader resolves binaries by earlyfs pathname at runtime (`role_image_bind`/`bind_role_image`, dynamically allocated per-segment frames via `elf64::load_dynamic()`) and a userspace `launch_path()` API can resolve and launch an arbitrary earlyfs-resident path at any point after boot, not just the six roles bound at startup; still bounded to a fixed 256 KiB/64-page image window and a single concurrently-resolvable dynamic role (`root_graph.hh::dynamic_launch_role`) -- lifting either is explicitly deferred, no current binary or use case needs it.
- [x] **USR-014** Per-segment W^X rejection, present-page overlap rejection, alignment validation, and explicit `ET_EXEC`-only acceptance (non-`ET_EXEC` types, e.g. PIE/`ET_DYN`, are now rejected by name in `valid_header()` rather than only incidentally via address-range math) are enforced by both `elf64::load()` and `elf64::load_dynamic()`. A permanent one-page unmapped guard gap is now reserved between an image's highest loadable page and the user stack (`elf64::loadable_pages`), closing a prior gap where a maximal-size image left zero gap before the stack.
- [-] **USR-015** argv/envp construction is now real, not deferred: `abi::v1::encode_process_args()`/`process_args_header` (`include/abi/sys/v1/process.hh`) encode a flat argv+envp block into a spawner-minted frame at a fixed address, and `process_entry.cc`'s `build_vectors()` decodes it into `sys_user_main(argc, argv, envp)` before a program's own code runs. The real consumer this item was explicitly waiting on now exists -- see USR-039's shell and coreutils, which parse real argv and read/write a real `environ`. TLS and a true ELF auxiliary vector remain explicitly deferred: still zero consumer needing thread-locals, and argc/argv/envp cover everything every current program actually does with its arguments. The stack-guard-gap half of this item is done (see USR-014).
- [ ] **USR-016** Dynamic linker: explicitly deferred, no current consumer, per this item's own "implemented or explicitly deferred" allowance.
- [x] **USR-017** `sys::root_graph::launch_path()` resolves an arbitrary earlyfs-resident path at runtime and launches it via the existing `role_image_bind` + `process_create` operations, with zero kernel/ABI changes -- verified end to end against a real release boot (dynamically bound `bin/control-plane` under a fresh role, process created and ran without disrupting the rest of boot). Scoped to earlyfs-resident paths only, and to one concurrently-resolvable dynamic role (see USR-013 note).
- [-] **USR-018** Root now drains its own fault endpoint each supervision-loop iteration (bounded 1-tick `ipc_receive`, previously never called by anything in userspace) and replies `terminate` immediately, so a crashing child is reaped promptly instead of silently sitting `blocked_fault` for the kernel's full 500-tick fault timeout. Visibility (an actual crash *report*, as opposed to prompt termination) remains open: production userspace has no logging syscall today, and OBS-010 forbids raw fault address/PC/syndrome in release logs even if one existed, so a redaction-compliant reporting mechanism needs its own design pass. Restart-on-crash now builds on this and is complete (USR-034); note that fault draining is owned exclusively by root's supervision thread once that thread exists, rather than by the readiness loop, so the two never race to reap or restart the same role. USR-034 also fixed a real defect in this item's own baseline: memory-server's production loop was receiving on the same endpoint object as root's fault endpoint and won most fault-delivery races, meaning "root reaps a crashing child promptly" did not reliably hold in a release build before that fix.
- [x] **USR-039** A real POSIX-style shell and seven coreutils run as ordinary forked, exec'd processes against a real VFS. `bin/sh` (`src/user/programs/sh/main.cc`) supports pipelines, input/output redirection (`<`, `>`, `>>`), `$VAR`/`$?` expansion, quoting, and in-process builtins (`cd`, `pwd`, `exit`, `export`, `echo`); `cat`, `echo`, `head`, `wc`, `ls`, `true`, `false` run as separate binaries resolved through `execv()` against a fixed, boot-bound role table (`src/user/include/sys/coreutils.hh`) rather than by loading arbitrary ELF bytes off disk at exec time -- binding a fixed set in advance is what lets a forked child run a *different* program without every process needing root's own image-binding authority; loading an arbitrary path at exec time remains open, the same explicit-deferral shape as USR-013/USR-016. Pipelines run sequentially through numbered ramfs temp files, not concurrently, because this kernel has no pipe object -- documented as a real limitation in `sh/main.cc`, not hidden behind the interface.

  Making fork+exec survive real use, rather than just the certification harness's own narrow fork/exec self-checks, required fixing four kernel defects, each only reachable once a process both forks and execs: a fault-terminated thread never published `exited`/`exit_status`, so `process_wait` spun forever on a child that had crashed; capability slot 16 silently kept naming the parent's args frame after fork while `clone_mappings()` had already mapped a fresh private copy at that same address; `exec_user_image()`'s `reclaim_task_memory()` freed the args frame `execv()` had just written the new argv into, because reclaim tracks ownership, not liveness; and `reap_user_bundle()` both failed closed on a forked child's slot 15 (which names the *parent's* shared memory resource, not the child's to destroy, leaking a task slot on every fork+reap until the fixed 16-task pool ran dry) and self-deadlocked on the authority lock by calling the locking `delete_capability()` from a scope that already held it -- a defect only reachable once the slot-15 failure stopped masking it first.

  Verified against a real `configs/release_defconfig` boot: `cat /etc/motd | wc` reads a real ext2 file through two forked, exec'd `cat`/`wc` processes chained via a ramfs pipe file and reports correct line/word/byte counts; redirection (`>`, `<`), directory listing (`ls`), and builtin/external interleaving all produce correct output. Covered automatically going forward by `make smoke`'s shell profile (TST-011) and by kernel certification (`[ACCEPTANCE] result=PASS`). Explicitly still open: loading an arbitrary path at exec time (USR-013), a real kernel pipe object, and job control.

## 7.4 Device and IRQ management

- [-] **USR-019** No generic device resource database exists; `memory::create_device_frame()` now rejects a second live device frame at the same physical address (a linear scan of `frames[]`, closing a real gap where two callers could otherwise both get capabilities to the same MMIO page), which is minimal exclusivity tracking, not a registry. A real database (enumerable inventory, ownership queries) remains open.
- [-] **USR-020** MMIO delegation exists only as the pre-existing single-purpose `device_frame_create` + `capability_mint` mechanism, now exclusivity-checked (USR-019); no generic broker/policy layer. The one production consumer (the guest's UART) no longer uses it at all -- it was converted to trap-and-emulate (vPL011) specifically to resolve the console-server/guest ownership conflict the exclusivity check surfaced, since a single physical UART cannot be safely handed to two direct-passthrough owners. `domain-manager` decodes guest MMIO exits (`exit.qualification`, not `exit.fault_address` -- the latter is unreliable for stage-2-only faults) and emulates PL011 register semantics, forwarding real character I/O through the console-server.
- [ ] **USR-021** IRQ broker implemented. Still open as a *broker*: there is no generic registration/arbitration layer. Two concrete userspace IRQ consumers now exist and share the same fixed pattern (root-gated `interrupt_create` in root, `capability_mint` into the owner, then `interrupt_bind` to a notification and `interrupt_ack` after servicing) -- the domain manager's manifest-driven guest IRQ forwarding, and the serial driver's real PL011 RX interrupt (USR-022). Both hardcode their own IRQ numbers and own their own notification; nothing enumerates, arbitrates, or delegates IRQs as a policy layer, which is what this item requires. Storm handling is now complete in both directions, which it was not: containment (mask past `storm_threshold` deliveries in a `storm_window_ticks` window) had no counterpart recovery, so the `stormed` flag was a one-way latch that killed a line for the remaining uptime after a single burst of legitimate traffic. `interrupt::recover_stormed()`, swept per timer tick, re-arms a line once its window has elapsed, turning containment into a rate ceiling rather than a permanent kill; it must be timer-driven because the mask being recovered from is what suppresses the delivery that would otherwise trigger recovery. Proven by `irq_storm_recovery`, which asserts both halves -- the previous test asserted containment only and passed either way. See evidence 0139.
- [x] **USR-022** Userspace UART driver implemented, and now split out of the console service into its own PL3 process (`src/user/drivers/serial/main.cc`, role `0x107`, wired like memory-server via a direct `process_create` outside the fixed five-slot `control_plane_role` loop since it is not a control-plane role). It exclusively owns the physical PL011: claims the root-minted device frame (`device_frame_create` is root-gated), configures `CR = UARTEN|TXE|RXE` (QEMU's PL011 model accepts TX regardless but gates RX behind `CR.RXE`), and drives real hardware with zero kernel `printk` involvement. **RX is now interrupt-driven rather than polled**: the driver unmasks `IMSC.RXIM`, binds a root-minted IRQ capability to its own notification (`interrupt_create` is root-gated, `interrupt_bind`/`interrupt_ack` are not), and drains the hardware FIFO into a 64-byte ring buffer only when the GIC has actually signaled, instead of re-reading `FR.RXFE` on every loop wakeup. The IRQ number was read off this platform's real device tree rather than assumed -- `pl011@9000000` declares `interrupts = <0 1 4>`, i.e. GIC SPI 1 = INTID 33, level-triggered. Proven end to end: with the driver instrumented, typing `help` at the guest shell fired the interrupt exactly once and drained exactly five bytes (`help\r`), which also demonstrates why the ring buffer is required -- the previous single-pending-byte scheme would have dropped four of those five. Serves a private `serial_operation` ABI (`include/abi/sys/v1/serial.hh`) to its one client, the console server. **RX now sleeps on the interrupt rather than polling for it**: `notification_bind`/`notification_unbind` (`control_operation` 52/53) let a thread bind a notification to itself so a blocked `ipc_receive()` also wakes when that notification signals -- the driver binds its IRQ notification once at startup and runs a single unbounded `ipc_receive`, branching on the new `error_t::notification_signal` status to distinguish a hardware wake from a real client request. The blocking behaviour is a SEPARATE operation, `serial_operation::read_byte_wait` (and `control_plane_operation::read_byte_wait` in front of it), not a change to `read_byte`: a `read_byte_wait` that finds the ring empty is answered later (kernel-retained reply capability, no extra userspace bookkeeping needed) once a subsequent interrupt actually delivers data, while plain `read_byte` keeps its original instant "none available" reply. That split is load-bearing and was found by regression, not designed up front -- making `read_byte` itself block silently broke its other caller, `domain-manager`'s `forward_console_input()`, which polls console input on every guest VM idle exit and depends on an immediate reply to hand control straight back to the vCPU; with it blocking, the guest-serving thread stalled until a real keystroke arrived, freezing the Zephyr guest (including its own timer processing) for seconds at a time. This directly fixed the reported bug: an idle interactive shell was measured pinning the qemu process at 115% host CPU because the shell's own `read()` (`src/user/lib/libc/io.cc`) retried a blocking `console::read_byte()` call in an unbounded loop every time the driver's old 1-tick bounded poll came back empty, which was also why a real keystroke felt delayed -- it queued up behind however many stale polls were already in flight. That retry loop is gone (`read()`'s `console_in` case is now a single `read_byte_wait` call). **The driver is correspondingly split across two threads and two endpoints**, for the reason the deferral itself creates: a parked reply lives in the serving thread's single `thread::reply` slot, which `install_reply()` overwrites on the next incoming call, so a `write` arriving while a read was parked destroyed the reader's reply capability and stranded every console reader permanently. Writes stay on `serial_service_endpoint`; `read_byte`/`read_byte_wait` moved to a new `serial_rx_endpoint` served by a second thread (reserved role `0x10b`, bound to the driver's own binary -- the same `thread_create` mechanism console-server uses for its stdin thread) which owns the RX half outright: the bound IRQ notification, the ring buffer, `drain_rx()`, and both read operations. Root mints that endpoint to console-server alone, whose stdin thread is itself a single thread blocking in `ipc_call`, so at most one reply is ever owed and one slot always suffices; both invariants are enforced by the capability graph and documented at `serial_rx_endpoint` rather than left implicit in driver code. The split also surfaced that `thread_create` gives each sibling thread its own address space (only the cspace is shared), so the RX thread must map the UART device frame into its own space -- `memory::map()` keys mapping records on (space, address) and supports this directly. See evidence 0137. Not covered by restart-on-fault (no `service_policy` entry), same boundary already drawn for memory-server.
- [x] **USR-023** Console server implemented (`src/user/servers/console/main.cc`): serves `health`/`describe`/`stop`/`write` (string)/`write_byte`/`read_byte` over IPC. It no longer touches hardware at all -- hardware ownership moved to the serial driver (USR-022) and the console server is now a pure IPC relay, **split across two independent threads**: the main thread serves `write`/`write_byte`/`health`/`describe`/`stop` on its existing service endpoint, and a second thread (spawned by the server itself via `thread_create` under reserved role `0x108`, bound to console-server's own binary -- the same mechanism root uses for its supervision thread) serves `read_byte` on a dedicated stdin endpoint. Neither thread shares state with the other; each only forwards its own operation type to the driver, so no cross-thread coordination is needed. `console_client.hh` keeps the same three functions -- only the endpoint value callers pass for `read_byte` changed, and the domain manager now holds a second console capability for it. Two genuine bugs surfaced during this split, both first reachable only because it is the first use of `thread_create` by a non-root task: boot hung nondeterministically (2 of 6 runs) because the stdin thread could spawn before root minted its endpoint, then spin hot on a failing capability resolution and starve its CPU -- fixed by waiting for the capability to resolve before spawning (6/6 after, 4/4 on re-check); and `destroy_user_bundle()` tore down only the thread named by `thread_selector`, leaving sibling threads of the same task pointing at a task object about to be unregistered and reused, which surfaced as a real `process_lifecycle_invariants` failure and is now fixed by tearing down every thread sharing the task. Originally verified end to end via `samples/guests/zephyr`'s `acceptance` target in release mode (`make MODE=release acceptance`, exit 0): guest boots, prints its banner, and its interactive shell responds correctly to a scripted `help` command through the entire real userspace-mediated I/O path (vPL011 MMIO trap/decode/resume, TX forwarding, RX polling, virtual IRQ 33 injection). Debug-mode (`CONFIG_VERBOSE_DIAGNOSTICS=y`) acceptance is functionally identical but its scripted exact-string grep fails on unrelated trace-log interleaving (`console_puts` in `arch/arm64/include/sys/arch/hypervisor.hh`'s per-guest-exit `[HV-TRAP]` diagnostic, pre-existing, now firing far more often since vPL011 makes every guest UART touch a real MMIO exit) -- not a functional gap, just a noisy debug config not designed for scripted matching. vPL011 TX now batches into a 23-byte local FIFO (`domain/main.cc`'s `vpl011::tx_buffer`) flushed via the console-server's existing string `write` op on buffer-full or guest idle (`wait`/`virtual_timer` exit), instead of one blocking IPC round-trip to the console-server per guest register write -- fixes serial output that was visibly slow under QEMU TCG emulation, since every character previously paid two full context switches. `forward_console_input()`'s RX poll (a blocking IPC call to the console-server) is now only invoked on genuinely idle exits (`wait`/`virtual_timer`), not on every mmio exit as it originally was -- measured via a host-side PTY timing harness capturing the `help` command's full response: unthrottled, 1219 bytes over 2.445s (499 B/s); idle-gated, the same 1219 bytes over 0.760s (1604 B/s, 3.2x). The guest's TX poll pattern (read `FR`, write `DR`) produces two mmio exits per output byte, so calling this on every exit taxed every single register access, not just genuine RX checks; gating it to idle exits keeps RX responsiveness between characters (the console-server and real hardware FIFO both cushion a few bytes typed mid-burst) without paying that cost during an active print. Remaining latency is consistent with QEMU TCG guest-CPU emulation speed (string formatting/shell processing between print segments), not I/O virtualization overhead -- not something this layer can improve further.
- [x] **USR-024** Driver crash and restart policy implemented. Both userspace drivers are now restart-covered, and the premise this item was originally deferred on turned out to be wrong. It claimed "every holder of a capability into a restarted driver would need a mid-flight re-mint"; in fact a client holds a capability to the service ENDPOINT OBJECT, which root created and owns and which survives the driver task's destruction untouched — exactly as `restart_role()` already documented for the five control-plane roles. Nothing on the client side is touched. Only the fresh child needs its own capabilities minted back into an empty cspace, and root holds every original, so `restart_service()` (root_graph.hh) is destroy + create + re-mint, parameterised per service by a `service_restart` descriptor. The create half (`device_frame_create`, `interrupt_create`) is deliberately *not* replayed — both reject a second live object for the same physical page or IRQ — which is precisely why root keeping the originals is what makes restart possible at all. What genuinely blocked this was in the kernel, not the capability graph: an interrupt whose owning task was destroyed mid-delivery kept `active` set with nobody left to acknowledge it, so `interrupt::bind()` refused with `busy` forever and a driver could never take its line back — unrestartable precisely when it most needed restarting. `bind()` now takes over a line whose bound notification no longer resolves (the owner is provably gone), deactivating it at the controller first so the fresh owner starts from a clean GIC state, and refuses only while the owner is still live and genuinely owes an acknowledge. Proven in the guest profile, which `make smoke` gates: `block-restart ok` destroys the block driver, brings it back, and re-runs the same write/read sector round trip, asserting the restarted driver reports the *same* result as before the restart rather than merely "not failed"; `serial-restart ok` restarts serial-driver and is written THROUGH the driver that was just restarted, so the marker appearing is itself the proof — a failed serial restart ends the boot log, caught as a missing marker rather than by a FAILED line it could not have printed. `guest alive via vpl011` still passes after both, so the whole console chain recovers. Covered by `irq_orphan_takeover` in the kernel suite (live owner → busy, orphaned → rebound with `active` cleared). A `vfs_service_restart` descriptor exists and shares the same path; it is unexercised, since VFS restart is not what this item names.
- [-] **USR-038** A second real userspace driver exists: the virtio-mmio block driver (`src/user/drivers/virtio/main.cc`, role `0x109`, wired like serial-driver via a direct `process_create` outside the fixed five-slot `control_plane_role` loop). Root creates the one live virtio-mmio device frame -- exclusivity-checked against the same physical page per USR-019 -- and mints it plus GIC SPI 79 and a serial-driver endpoint into the driver, which then binds the IRQ to its own notification and acks after servicing, the same fixed pattern USR-021 notes is still not a broker. It drives a real split virtqueue and serves a private `block_operation` ABI (`include/abi/sys/v1/virtio.hh`, same non-control-plane-role convention as `serial_operation`): `info` reports capacity and sector size, `probe` returns a transport's device id/version, and `read`/`write` move whole 512-byte sectors through a shared payload frame kept deliberately separate from the virtqueue ring page, so a client holding the payload capability cannot reach the ring. Verified from a real client rather than from inside the driver: `root_graph.hh::verify_block_service()` performs a write/read sector round trip and `make smoke` gates on `block-service verified`. Open: no partition or filesystem layer, no concurrent-client policy, and only one transport is claimed. Restart coverage landed with USR-024 -- this driver is the one the `block-restart ok` gate exercises.

## 7.5 Domain manager

- [-] **USR-025** Userspace domain manager/VMM implemented; PL3 domain-manager service, role-specific image loading, a dedicated load operation, VM launch/destroy request handling, and earlyfs packaging exist, but production guest deployment remains open.
- [x] **USR-026** VM creation uses capability-authorized kernel APIs. `sys::domain_manager::manager::create()` reaches the kernel only through `vm.create(vm_selector, vcpu_selector, logical_id, counter_offset)` -- selectors into the domain manager's own cspace, resolved and rights-checked by the kernel like any other capability invocation, never a physical address or an ambient VM index. The same holds for the rest of the lifecycle: `map_frame(ipa, frame_capability, permissions)` accepts an authorized frame capability and nothing else (HYP-015), and configure/run/pause/resume/stop/destroy all go through the vCPU capability. A domain manager holding no VM capability can create no VM. Covered by `domain_manager_api`, `hypervisor_vm_create` and `hypervisor_vcpu_create` in certification, all PASS.
- [x] **USR-027** Guest image loading performed in userspace. `load_guest_image()` in `src/user/servers/domain/main.cc` does the whole job at PL3: validates the ELF identity, machine and version, walks the program headers, derives per-page W^X permissions from the section flags (`elf_page_flags()`), and maps each page into the guest through `vm.map_frame()` with an authorized frame capability. The kernel parses no guest image and knows nothing about ELF on the guest path; it is handed IPA/frame/permission triples. A malformed image is a userspace error with a reported failure code, not a kernel one. Covered by `domain_guest_load` in certification, and exercised on every `make smoke` guest boot, which loads a real pinned Zephyr v4.0.0 image this way (USR-032).
- [x] **USR-028** VM memory and device assignment policy remains in userspace. The guest's layout is decided by a userspace manifest (`src/user/include/sys/guest_manifest.hh`): `map_manifest_devices()` walks it and, per device, chooses the guest IPA, the mapping permissions, and whether a host IRQ is forwarded into the guest. RAM size, entry, PSTATE and stack come from the same manifest. The kernel supplies mechanism only -- it maps the frame capability it is given at the IPA it is told, and rejects anything unauthorized -- while root supplies the device capabilities and the domain manager decides what the guest sees. No assignment policy lives in the kernel. Exercised on every guest boot: the delegated PL011 and its forwarded interrupt reach the guest entirely through this path.
- [-] **USR-029** VM lifecycle exposed through stable management API; `sys::domain_manager::manager`, the domain-role control-plane request path, and the dedicated load op cover create/destroy in certification, but the production management protocol remains open.
- [ ] **USR-030** Linux guest launch demonstrated. Open, and a genuine feature rather than bookkeeping -- worth stating what is actually missing so the size is not mistaken. Zephyr boots (USR-032) because the manifest hands it exactly what it needs: one delegated PL011, the virtual timer, and a single forwarded IRQ. Linux needs materially more from the host: PSCI (at minimum `CPU_ON`/`CPU_OFF`/`SYSTEM_OFF`) for secondary CPU bringup, which nothing here emulates; a GIC distributor and redistributor visible to the guest as MMIO, whereas HYP-025's virtual controller is bounded and vCPU-resident with no distributor emulation; a device tree constructed for the guest, which the manifest has no notion of; and a root filesystem, meaning either an initramfs the loader can place or a virtio-blk device emulated for the guest. Each of those is independently substantial, and the SMMU blocker (DEV-007) does not apply here only because a Linux guest would use emulated rather than assigned devices.
- [ ] **USR-031** BSD guest launch demonstrated. Open, with the same prerequisites as USR-030 (PSCI, guest-visible GIC distributor/redistributor MMIO, a constructed device tree, and a root filesystem path) plus whichever boot protocol the chosen BSD expects. Not attempted; USR-030 is the one worth doing first, since everything it needs is shared.
- [x] **USR-032** Pinned Zephyr v4.0.0 boots through the PL3 domain manager with section-level W^X loading, bounded vGIC/timer support, delegated PL011, and a native interactive shell that accepts `help` and returns the command list.

## 7.6 Supervision

- [x] **USR-033** Root launches the bounded six-service graph, retains lifecycle capabilities, monitors readiness/failure badges, actively probes every private service endpoint, and now monitors exit status. A role that EXITS is not a role that faulted, and only the latter was noticed: the supervision thread watches the fault endpoint, which a clean `thread_exit` never touches, and the readiness loop checked the failure badge and nothing else -- so a service that returned from main (console-server's `stop` path does exactly that) simply vanished, leaving its endpoint unanswered with no restart and no report. `handle_role_exits()` now reads the per-role exit badges out of the same notification the readiness loop already polls, clears that role's stale readiness bit, and restarts it through the same bounded admission control as a fault restart, so a zero-limit role stays dead and a flapping one stops rather than looping. Driven from the readiness loop rather than the supervision thread deliberately: that thread blocks indefinitely on the fault endpoint, which is what stopped it waking every tick, and giving it a second thing to watch would mean returning to polling. Fixing this also uncovered a live badge collision -- `control_plane_exit_badge()` started at bit 8, the same bit as `vfs_service_ready_badge`, so a process-role exit and a VFS readiness signal were the same bit in a shared accumulated word; it went unnoticed precisely because nothing read exit badges. Exit badges now start at bit 16 and `static_assert`s keep the readiness, exit and failure classes disjoint by construction. Gated by `exit-restart ok`, which sends a real `stop` to the device role and requires the restarted instance to come back and answer a health RPC.
- [x] **USR-034** Bounded per-role restart admission and real destroy/recreate/remint/health recovery run in production, driven by unexpected faults. Root's supervision thread (the second thread `thread_create` gives it) decodes each fault's sender badge back to a thread id via `endpoint_badge()`'s inverse, correlates it against a per-role table in a shared page (`root_graph.hh`'s `supervisor_state`, genuine shared memory because root's two threads share a cspace but not an address space), and calls `restart_role()`: `process_destroy`, `process_create` at the same selectors, re-mint of the role's own service endpoint, plus that role's extra minting (console's UART frame; the domain's device/IRQ/console capabilities followed by `launch`+`load`). Every other role's capability into a restarted role stays valid untouched, since root's copy of the service-endpoint object outlives the destroyed task -- only the fresh child needs a re-mint into its own empty cspace. Restart admission is `control_plane::may_restart()` against `policy_for()`'s per-role `restart_limit`. Verified by injecting real illegal-instruction faults in a release build: device crashed and came back healthy, and the domain-manager crashed and the Zephyr guest genuinely re-booted to an interactive shell twice, then correctly stayed down on the third crash (domain's `restart_limit=2`). Three genuine, previously-latent bugs had to be fixed to make this work, each only reachable once something actually restarted a role: (1) `destroy_user_bundle()` never woke a caller blocked waiting on the destroyed thread's reply -- `thread_exit()` did this for a *graceful* exit but a forced destroy did not, so destroying the domain-manager would have hung root's `serve()` call forever (extracted as `thread::release_pending_reply()`, now shared by both paths, with its own certification test); (2) `destroy_user_bundle()` leaked every frame and page table the task had allocated, because both pools are only ever returned by an explicit `destroy_frame`/`destroy_page_table` from the owning task -- with just 64 frame slots the domain-manager's guest-image staging exhausted the pool on the very first restart and the fresh guest load failed `no_memory` (fixed by `memory::reclaim_task_memory()`); (3) memory-server's production request loop received on slot 10, which is the same object as root's fault endpoint, so it sat permanently `blocked_receive` there and won essentially every fault-delivery race for *any* role's crash -- it now has its own service endpoint (`memory_service_endpoint`), the same convention every other role already used. `process_create` additionally now returns the thread id it allocated (via `frame.x[1]`, no ABI enum change), which is what makes badge-to-role correlation possible at all. Out of scope and still open: memory-server restart (no `service_policy` entry; other roles hold live capabilities to frames it manages), crash *reporting* (no userspace logging syscall, and OBS-010 forbids raw fault details in release logs), and recovery from a non-fault terminal guest exit.
- [x] **USR-035** Core roles have an explicit bounded dependency mask and root launches process, device, console, domain, then supervisor in dependency order.
- [-] **USR-036** Per-role restart limits fail closed and zero-limit services cannot restart; unexpected crash accounting is now real (`supervisor_state.roles[].restart_count`, incremented per fault-triggered restart and checked by `control_plane::may_restart()` -- verified by repeat-crashing the domain role until it correctly stayed down at its `restart_limit=2`, see USR-034). Time-windowed backoff remains open: `restart_count` is a monotonic attempt counter with no delay between attempts and no decay over time, so a role that crashes slowly over hours is treated the same as one crash-looping in milliseconds.
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

- [x] **DEV-001** Device ownership represented by capabilities. A device is owned by holding capabilities to it and by nothing else: the MMIO page is a `frame` object with `device=true`, created only by a root task (`device_frame_create`) and exclusivity-checked so only one live device frame may exist per physical page; the line is an `interrupt` object created only by a root task (`interrupt_create`) and registered against a single IRQ number. Neither is reachable by a task that does not hold the capability — `map_frame` and `interrupt_bind` both resolve through the holder's cspace with the required rights — and both are delegated by `capability_mint` and withdrawn by `capability_revoke`, with revocation now severing the device's live effects as well as its name (DEV-003). There is no ambient device registry a task can address by number instead.
- [x] **DEV-002** MMIO regions delegated safely. Delegation was already sound: an MMIO page is reachable only through a root-created `device_frame_create` capability, exclusivity-checked so two callers cannot both be granted the same physical page, minted onward with explicit rights, and mapped only with device memory attributes that `memory::valid_attributes()` enforces for any frame carrying the device flag. Revocation was the hole, exactly as for IRQs: clearing the cspace slot left the page still MAPPED in the old owner's address space, so it could no longer name the device but could still drive it. `memory::release_frame_mappings()` now drops every mapping of the frame across whatever spaces hold them — the inverse of `unmap_all()`, which drops every mapping of one space — driven by the same post-revoke hook as DEV-003. Restricted to device frames deliberately: an ordinary shared frame (VFS's client buffer, the block driver's payload page) legitimately has several holders, and revoking one holder's capability must not unmap it from the others, whereas a device frame is exclusivity-checked to a single live owner so reclaiming it means precisely this.
- [x] **DEV-003** IRQs delegated and revoked safely. Delegation was already real (root-gated `interrupt_create`, `capability_mint` into the owner, `interrupt_bind` to the owner's own notification, `interrupt_ack` after servicing), and `irq_ownership_delegation` has covered it. Revocation was NOT: `revoke_descendants()` cleared the cspace slot and nothing else, so the interrupt object stayed bound to the old owner's notification and stayed unmasked — the device kept firing into a driver that no longer held a capability to it. The name was revoked; the authority was not. Capability revocation now runs a hook (`interrupt::on_capability_revoked`) that masks the line at the controller, drops any in-flight delivery, and unbinds it, so re-delegation starts from a clean state via `bind()`. The hook is invoked after every cspace lock is released, because the device teardown it performs takes locks that rank below them and would otherwise invert the order; `cspace.hh` deliberately does not know which object types own hardware, so the type filter lives in `interrupt.hh`. `irq_ownership_delegation` now asserts the severed state (masked, not active, unbound) rather than only that the capability lookup fails.
- [-] **DEV-004** Device reset requirement documented per device. Documented, not implemented, and the distinction is the point of this entry. Two devices are assignable today. PL011 UART (`serial-driver`): reset requirement is to mask RX at IMSC, drain the RX FIFO, and clear pending interrupts at ICR before a new owner binds; the kernel now performs the interrupt half on revocation (`interrupt::release_ownership()` masks the line, drops any in-flight delivery and unbinds), and the FIFO half is done by the incoming owner's `configure_uart()`/`drain_rx()` rather than by the outgoing one. virtio-mmio block (`virtio-driver`): reset requirement is a write of 0 to `Status`, which per the virtio spec resets the device and abandons the virtqueue, before a new owner re-negotiates; this is NOT performed on revocation — a restarted driver currently re-initialises the transport from whatever state the previous owner left, which works because it rewrites `Status` during its own bring-up, but is not the same as the host guaranteeing a clean device. Neither device is reset by the kernel on reassignment, so state leakage between owners is prevented only by each incoming driver re-initialising its device. That is what DEV-017 asks to be proven and why it stays open.
- [x] **DEV-005** Assignment rollback implemented. Provisioning a service creates the task first and then mints its capabilities, and a failure anywhere after the create used to simply return, leaving the half-provisioned task alive: a stranded thread slot, cspace and memory resource, and — worse — holding whatever device capabilities did land, so the device looked busy to the next assignment attempt while nothing was driving it. `restart_service()` now destroys the fresh task on any provisioning failure. Rollback is one operation rather than an undo list, because destroying the task frees its whole cspace, and since revocation severs device authority as well as naming (DEV-001..003) the frame and line come back genuinely reclaimable rather than merely unnamed. Two supporting corrections fell out of testing it: the opening `process_destroy` is now best-effort, since it establishes a precondition ("nothing at these selectors") rather than performing an operation whose failure matters — treating an already-free slot as an error made recovery impossible exactly after a rollback; and every path that frees a space's pages now identifies itself to the allocator barrier (`release_space_pages()`, and the `clone()` call in fork), without which a space handing back its OWN pages was mistaken for one stealing them from a live holder and 23 pages leaked per fork-bearing boot. Gated by `assign-rollback ok`, which induces a real partial assignment — a descriptor whose service endpoint names a vacant root slot, so `process_create` succeeds and the first mint fails — and whose completeness is proven by the `block-restart ok` that follows: a successful restart at the same selectors is only possible if the rollback actually freed them.

## 9.2 SMMU

- [x] **DEV-006** ARM SMMU discovery implemented. The device tree is searched for a node whose `compatible` names `arm,smmu-v3` and that node's `reg` window is retained (`boot::fdt::inventory::smmu`, `memory::discovered_smmu`); the kernel then reads IDR0/IDR1/IDR5 out of it and reports what the implementation supports. Matched on the binding string rather than the node name, since the name carries the base address and differs between machines. Verified both ways on the machine `tools/run/run.sh` boots: without an SMMU it reports `smmu: absent (no arm,smmu-v3 node)`, and with `ZILCH_SMMU=1` (which adds `iommu=smmuv3` to both the dumpdtb and run invocations, so the blob the kernel parses describes the machine it is running on) it reports `arm,smmu-v3 at 9050000 idr0=d44101b idr1=2730010 idr5=74 stage1=1 stage2=1 sidbits=16 translation=off` — S1P and S2P both set and SIDSIZE=16, which is exactly QEMU's SMMUv3. Identification registers only; no write touches the device. The SMMU is deliberately kernel-owned rather than delegated to a userspace driver like every other device here, because it is what makes that delegation safe: a driver holding it could remove its own containment. `translation=off` is reported honestly — see DEV-007 for why the translation path is not reachable on this platform.
- [ ] **DEV-007** Stream ownership database implemented. Blocked on a prerequisite this platform does not provide, and the blocker is shared by DEV-008 through DEV-018 so it is recorded once here. On QEMU's `virt` machine the SMMUv3 fronts the PCIe root complex and nothing else: the device tree gives `iommu-map` to `pcie`, and not one of the thirty-two `virtio_mmio@...` nodes carries an `iommus` property — verified by dumping the DTB of the very machine `tools/run/run.sh` boots. This kernel's only real device is virtio-mmio, which bypasses the SMMU entirely. A stream table, per-domain translation context or invalidation path built now would translate for no device, and every assertion about it would be vacuous — which the release evidence section already forbids ("no mandatory feature relies on a model, mock, fixture, or hard-coded resource pool"). Reaching the translation path needs a DMA-capable device behind the SMMU, which on this machine means PCIe, which means a root-complex driver: ECAM enumeration, BAR assignment, and MSI through the ITS. That subsystem does not exist here and is not itself listed as a requirement. DEV-006 (discovery) is done because it is the part that can be exercised against the real device.
- [ ] **DEV-008** Per-domain translation context implemented.
- [ ] **DEV-009** DMA mappings tied to VM capabilities.
- [ ] **DEV-010** SMMU invalidation and synchronization implemented.
- [ ] **DEV-011** DMA blocked before memory/device reuse.
- [ ] **DEV-012** Fault reporting and containment implemented.
- [ ] **DEV-013** Interrupt remapping implemented where required.

## 9.3 Production assignment tests

- [ ] **DEV-014** Assigned device cannot DMA into kernel memory. Requires a DMA-capable device behind the SMMU to assert against; see DEV-007 for the platform blocker shared by this whole group.
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
- [x] **OBS-011** Formatted kernel records use Linux-style boot-relative `[    seconds.microseconds]` timestamps from the calibrated architectural counter; SMP record serialization includes the timestamp and severity prefix. Evidence: `printk.hh`'s `CONFIG_PRINTK_TIME` path, enabled by both `debug_defconfig` and `release_defconfig`; every certification and release record emits as `[    5.271927] [INFO] ...`.

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
- [x] **TST-011** Real userspace service graph integration test exists: `tools/verification/smoke.sh` (`make smoke`) boots the two profiles the certification suite structurally cannot reach, because `CONFIG_SELFTEST=y` replaces init's `main()` and therefore never runs `root_graph.hh`'s `supervise()` -- the service graph that actually ships. It builds and boots `configs/release_defconfig` and `configs/guest_defconfig` under bounded QEMU and asserts on end state, not exit code (these profiles run forever by design): `graph ready`, `console-server alive`, `block-service verified` for the release graph, and `graph ready`, `restart ok`, `guest: loaded, serving`, `guest alive via vpl011` for the guest profile, with an explicit failure-marker list so unrelated output cannot make the assertions vacuous. This is a production-profile boot test, not a self-test, so the "no self-test counts as completion" rule does not apply. It exists because the release profile once booted for some time with its supervision thread failing to spawn -- and therefore no restart-on-fault -- while the console log looked healthy.

  A third profile now scripts a real shell session and asserts on its transcript (USR-039), the one thing no boot-marker check above can prove: that a command typed at the console actually forks, execs, and produces correct output through a real pipeline and a real redirection, not just that root's own boot-time probes ran. It boots with no disk attached (`BLOCK_IMAGE=-`) so it never depends on virtio's completion wait -- a fixed-iteration poll with no blocking primitive that times out under host contention on its own schedule, unrelated to anything this profile exercises -- and pipes three commands into the console over a FIFO, paced with short sleeps between them: writing the whole script as one burst overran the console's byte-at-a-time IPC read path and silently stalled the shell's own `read_line()`, confirmed by direct reproduction. Asserts the redirected/piped marker string appears exactly the expected number of times (once in the console's own echo of the `echo ... >` command line, once from a direct read, once from a piped read), not just that it appears at all.
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

- [x] Hypervisor object/lifecycle gate — composes 8.1 (HYP-001..007, all complete). Certification: `hypervisor_vm_create`/`vm_destroy`/`vm_stale`/`vm_reuse`/`vm_parent_busy`, `hypervisor_vcpu_create`/`vcpu_destroy`/`vcpu_stale`, `hypervisor_dynamic_lifecycle` — all PASS.
- [x] Stage-2 translation gate — composes 8.2 (HYP-008..015, all complete), whose evidence is recorded per requirement; exercised in certification through `hypervisor_control_model_0_4`/`_0_5`/`_0_6` and the real-execution tests below, which cannot run without stage-2 descriptors being populated correctly.
- [x] Real multi-vCPU execution gate — composes 8.3 (HYP-016..022, all complete). Certification: `hypervisor_real_single_vcpu`, `hypervisor_real_smp_execution` — both PASS.
- [x] Production virtual interrupt gate — composes 8.4 (HYP-025..033, all complete), whose evidence is recorded per requirement; the real guest additionally takes virtual IRQ 33 through this path on every `make smoke` guest profile boot (`guest alive via vpl011`).
- [x] Production virtual timer gate — composes 8.5 (HYP-034..040, all complete). Certification: `virtual_timer_lifecycle result=PASS expirations=1 cancellations=1 generations=3`, plus `timer_database_invariants`.
- [x] Concurrent multi-VM gate — composes HYP-023 and HYP-024. Certification: `hypervisor_real_multivm_isolation result=PASS`.
- [x] Guest fault-containment gate — composes 8.6 (HYP-041..047, all complete) and HYP-024. Certification: `hypervisor_negative_fuzz` and `hypervisor_real_multivm_isolation` — both PASS.
- [ ] Userspace VMM/domain-manager gate — NOT met: section 7.5 has 5 open and 2 partial requirements. `domain_manager_api`, `domain_guest_load` and `domain_guest_run` pass, and the guest profile boots a real Zephyr guest to an interactive shell, but the section's own requirements are not complete.
- [ ] Device assignment and SMMU gate — NOT met: 9.1 is complete and DEV-006 (discovery) is done, but DEV-007..018 are blocked on this platform. See DEV-007 for the blocker — QEMU's virt SMMUv3 fronts the PCIe root complex only, and this kernel's devices are virtio-mmio, so there is nothing behind the SMMU to translate for.
- [ ] Security and teardown gate — NOT met: 10.1, 10.2 and 10.3 are complete and HYP-007 covers teardown serialization, but 10.4 still carries 2 partial requirements.
- [ ] Stress, fuzz, and soak gate — NOT met: 12.3 has 2 open and 4 partial, and 12.4 (long-duration certification) has 5 open. `hypervisor_negative_fuzz`, `root_created_smp_fuzz`, `cross_cspace_transfer_fuzz` and `rt_logical_time_soak` pass, which is not the same as the section being complete.
- [ ] Real hardware ARM64 certification gate — NOT met, and not reachable from this environment: every result in this document comes from QEMU. This needs the kernel booted on physical ARM64 hardware.

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

These phases track *implementation milestones* and are subordinate to the
per-requirement sections above; where a phase and an item section disagree,
the item section governs. Phase status is derived from the completion gates
in sections 2 through 8, not asserted independently.

## Phase A — restore architectural discipline

- [x] A1. Add test-only configuration boundaries.
- [-] A2. Test operations are configuration-guarded; final production ABI cleanup and binary audit remain open.
- [x] A3. Split hypervisor implementation into production modules. Covered by PRD-013 through PRD-016: architecture-independent VM/vCPU objects, stage-2, virtual IRQ/timer, and lifecycle/VMID are each isolated in their own module.
- [-] A4. Stable requirement IDs now exist; implementation/test/evidence links must be populated.
- [-] A5. Renaming tracks DOC-005: model-only results now use `HV-MODEL`/`hypervisor_control_model`, but legacy profile documents still require complete renaming and archival.

## Phase B — complete kernel mechanisms

- [x] B1. Capability derivation and revoke.
- [x] B2. Complete IPC, reply objects, transfer, timeout, cancellation. Covered by IPC-GATE.
- [-] B3. Fault IPC and a two-client pager protocol exist; failure/death/concurrency policies remain open.
- [-] B4. Allocator-backed frames/page tables and bounded reverse mappings exist; full root delegation and pressure evidence remain open.
- [x] B5. Production RT scheduler and scheduling-context donation. Covered by SCH-GATE.

## Phase C — build the userspace OS

- [-] C1. Root resource manager launches and supervises the production service graph (USR-001, USR-003, USR-034, USR-035); explicit delegation of all allocatable RAM, a unified external management endpoint, and exit-status monitoring remain open (USR-002, USR-004, USR-005, USR-033).
- [-] C2. Independent memory-server/pager test service exists; production service API and policies remain open.
- [-] C3. Earlyfs paths resolve at runtime and `launch_path()` can launch an arbitrary earlyfs-resident image (USR-013, USR-017); a general loader is still bounded to a 256 KiB image window and one concurrently-resolvable dynamic role, and TLS/auxv remain deferred -- argv/envp construction is now real and has a real consumer (USR-015, USR-039).
- [-] C4. Console server and two real drivers exist (USR-022, USR-023, USR-038); the device/IRQ *manager* does not -- no device database, no MMIO broker, no IRQ arbitration (USR-019, USR-020, USR-021), and no driver restart policy (USR-024).
- [-] C5. Domain manager/VMM and supervisor run in production with bounded restart admission (USR-032, USR-034); production guest deployment, device-assignment policy, and the production management protocol remain open (USR-025, USR-028, USR-029).
- [-] C6. A shell and seven coreutils run as real forked, exec'd userspace processes against a real ext2/ramfs VFS, scripted end to end by `make smoke`'s shell profile (USR-039, TST-011); loading an arbitrary path at exec time, a real kernel pipe object, and job control remain open (USR-013).

## Phase D — complete real hypervisor execution

Every item below is covered by HYP-EXEC-GATE. These are execution milestones
only; the Hypervisor 1.0 gate in section 14 additionally requires device
assignment, SMMU, soak, and real-hardware evidence, and remains open.

- [x] D1. Real secondary guest CPU entry. Covered by HYP-017.
- [x] D2. Four simultaneous EL2 guest execution loops. Covered by HYP-018 and HYP-019.
- [x] D3. Production virtual GIC and timer. Covered by HYP-025 through HYP-040.
- [x] D4. Real preemption and migration. Covered by HYP-021 and HYP-022.
- [x] D5. Concurrent multi-VM execution. Covered by HYP-023 and HYP-024.
- [x] D6. Secure teardown and VMID rollover. Covered by HYP-013 and TST-021.

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

<!-- 0124 evidence: elf64::load()/load_dynamic() now reject non-ET_EXEC ELF
types by name and reserve a permanent one-page unmapped guard gap below the
user stack (completes USR-014). sys::root_graph::launch_path() resolves an
arbitrary earlyfs-resident path at runtime via the existing role_image_bind +
process_create operations and was verified end to end against a real release
boot (advances USR-013, completes the mechanism half of USR-017 -- scoped to
earlyfs-resident paths and one concurrently-resolvable dynamic role). Root's
supervision loop now drains its own fault endpoint each iteration and replies
terminate immediately, reaping crashed children promptly instead of via the
kernel's full 500-tick fault timeout (advances USR-018 -- termination is
prompt, but visible crash reporting remains open pending a redaction-
compliant design, since production userspace has no logging syscall and
OBS-010 forbids raw fault detail in release logs regardless). TLS, argv/envp/
auxv, and the dynamic linker remain explicitly deferred (USR-015, USR-016):
no current consumer exists for any of them. -->

<!-- 0130 evidence: a real userspace UART driver now exclusively owns the
physical PL011 (completes USR-022/USR-023). It lived in the console-server
process when this batch was written; it has since been split into its own
serial-driver process with interrupt-driven RX, and the console server
split into stdin/stdout threads -- see USR-022/USR-023 above for current
behavior. The guest's
UART is no longer direct passthrough but vPL011 trap-and-emulate through
domain-manager, verified end to end via samples/guests/zephyr's `acceptance`
target in release mode (guest banner + interactive shell respond to a
scripted `help` command through the real MMIO-trap/decode/resume + IRQ-
injection path, exit 0). This was forced by a real, newly-closed gap:
memory::create_device_frame() previously let two callers both get
capabilities to the same physical MMIO page; once closed, the console-server
and the guest's old direct passthrough became mutually exclusive claimants
of the one physical UART, and vPL011 is the resolution (advances USR-019/
USR-020, not completing either -- there is still no generic device database
or MMIO delegation broker, just exclusivity-checked single-purpose
mechanisms). Fixed a genuine latent kernel bug found along the way:
vcpu_state_write's PC-field (31) write validation checked page alignment
instead of 4-byte instruction alignment, rejecting every legitimate
guest-resume PC write (vcpu.hh) -- never exercised before since nothing had
called it. Also fixed: the separate CONFIG_SELFTEST legacy test harness
(user/init/main.cc's test_domain_manager_lifecycle(), which never uses
root_graph.hh) needed its own equivalent console-server-endpoint wiring
after this change, since it constructs the domain-manager's control-plane
graph independently; its non-interactive `run` control-plane operation
needed a small internal loop to resolve vPL011 MMIO exits transparently,
restoring the single-call semantic passthrough used to provide for free.
Separately, default_manifest.cc (the built-in synthetic ARM64 verification
guest's manifest, unrelated to the Zephyr sample) was still declaring a
UART passthrough device it never actually touches (confirmed: no UART
reference anywhere in guests/test-arm64/entry.S) -- corrected to zero
devices, matching the guest's real needs. This batch also recorded that a
plain release build with no embedded guest (configs/release_defconfig,
CONFIG_GUEST_EMBEDDED_IMAGE unset) failed to link, because
domain-manager's forward_device_irqs() referenced the guest-manifest symbol
unconditionally. That is now fixed, and the no-guest release profile is a
gating build: tools/verification/smoke.sh builds and boots
configs/release_defconfig on every `make smoke` (see TST-011). -->

<!-- 0131 evidence: reconciliation pass only, no product change. The header
baseline had drifted 56 patches behind this file's own trailing evidence
notes. Verified against the tree and flipped: PRD-019, PRD-021, PRD-022, and
PRD-023 (the Kconfig hierarchy, release DWARF/debug gates, per-sample guest
enablement, and sample-local ownership all exist and were merely never
re-scored); OBS-011 (boot-relative printk timestamps ship in both
defconfigs); TST-011 (tools/verification/smoke.sh is a real production-
profile service-graph test, not a self-test). PRD-020 was downgraded from
NOT STARTED to IN PROGRESS -- the defconfigs exist but have not replaced the
BUILD_VARIANT set, so two selection mechanisms coexist. Section 15's phase
list contradicted the item sections it summarizes (B2/B5 unchecked against
completed IPC and scheduler gates; all of D1-D6 unchecked against a
completed HYP-EXEC-GATE; C1/C4 unchecked against completed root, console,
and driver items); phases are now explicitly derived from the completion
gates and subordinate to the item sections. Added USR-038 for the
virtio-mmio block driver, which was live in the production service graph
and gated by `make smoke` while having no requirement ID anywhere in this
checklist. Re-verified while scoring: certification ledger 144 PASS / 0
FAIL with root-only acceptance PASS, `make smoke` PASS across both
profiles, and amd64 still builds clean but cannot boot under tools/run
(QEMU multiboot is 32-bit only), so PLT-007's compile-only claim stands
unchanged. Note for anyone reproducing this: the build requires the repo's
flake devShell (`nix develop`); an ambient shell without it lacks kconfiglib
-- leaving autoconf.h stale, which surfaces as a -Wundef error on
CONFIG_FAULT_INJECTION -- and injects -fstack-clash-protection, which clang
does not implement for aarch64. -->

<!-- 0132 evidence: added USR-039 for a real POSIX shell and seven coreutils
running as forked, exec'd userspace processes against a real VFS -- like
USR-038, live in the production service graph and gated by `make smoke`
while having no requirement ID anywhere in this checklist until now. Fixed
four kernel defects to get there, all only reachable once a process both
forks and execs (a fault-terminated thread never publishing exited/
exit_status; capability slot 16 silently naming the parent's args frame
after fork while a private copy was mapped there; exec's
reclaim_task_memory() freeing the args frame execv() had just written into;
reap_user_bundle() failing closed on a forked child's shared slot-15
memory resource, which leaked a task slot on every fork+reap, and then
self-deadlocking on the authority lock once that failure stopped masking
it). Completes the argv/envp half of USR-015 -- deferred since batch 0124
for "zero current consumer"; the shell is that consumer, TLS/auxv remain
correctly deferred with none. Extended TST-011: `make smoke` gained a
third profile that scripts a real shell session over a console FIFO and
asserts on the transcript, rather than on a service reporting its own
readiness. Getting the profile itself right took two iterations: writing
the whole script as one burst before qemu started (reasoning that OS pipe
buffering would hold it) overran the console's byte-at-a-time IPC read
path and silently stalled the shell's own read_line() -- confirmed by
direct reproduction, fixed by pacing writes with short sleeps while qemu
runs in the background; and an initial marker-count assertion assumed the
marker string appeared in no command line, which was wrong for the `echo`
command whose own argument is that string. Re-verified while scoring:
certification `[ACCEPTANCE] result=PASS failures=0`, `make smoke` PASS
across all three profiles including a run immediately after a full clean
rebuild of every build tree, and no stray qemu process survives the new
profile's teardown. -->

<!-- 0133 evidence: added a bound-notification IPC wake primitive
(`notification_bind`/`notification_unbind`, `control_operation` 52/53;
`src/kernel/include/sys/kernel/notification/notification.hh`,
`thread/scheduler.hh`, `syscall/{control,ipc}.hh`) so a thread can bind a
notification to itself and have a blocked `ipc_receive()` wake on either a
real message or that notification's signal, distinguished via the new
`error_t::notification_signal` status rather than a reserved badge bit
(`capability::badge_t` is u64, and `ipc_result.status` was sitting unused
for exactly this). Two real concurrency bugs were found and fixed while
landing it, both only reachable under specific interleavings and neither
caught by a single green certification run: (1) `receive()`'s
blocking-commit path transitioned to `blocked_receive` under only the
endpoint's own lock, with nothing synchronizing a bound notification's
signal against that transition -- a signal landing in the window between
"decided to block" and "recorded as blocked" was silently dropped; fixed
by holding `lock_ipc_lifecycle()` across both sides of that race, endpoint
lock outer per the existing documented ordering. (2) a notification wake
pulls a thread out of `blocked_receive` without going through the normal
send()-finds-receiver path that clears the endpoint's `receiver` field,
which `kernel_lifetime_invariants`' endpoint check caught once as a stale
reference; fixed by snapshotting the thread's endpoint registration under
the lock and clearing it via the existing `ipc::cancel_thread()` after the
lock is released (endpoint locks are never nested inside
`lock_ipc_lifecycle()`, so the clear cannot happen synchronously without
risking an AB-BA deadlock against `receive()`'s own path). Adopted in the
serial driver (USR-022) and the shell's `read()` (`src/user/lib/libc/io.cc`),
eliminating the specific busy-poll that was measured pinning an idle
interactive shell's qemu process at 115% host CPU. Verified: the new
certification race test (`test_notification_bind_wake`,
`src/user/tests/certification/main.cc`) passed on every run; the full
certification suite was run ~20 times total during this work, all
`[ACCEPTANCE]` failures traced to either wall-clock latency-threshold
assertions or one dangling-sender endpoint state, both reproduced on a
clean pre-change baseline under the same (real, shared, non-dedicated)
host and confirmed unrelated to this change; `make smoke` passed across
all three profiles (one transient miss on the shell profile's
timing-paced marker count reproduced as a clean pass on immediate re-run
under identical host load, consistent with the profile's own documented
host-load sensitivity, not a functional regression). Found while
measuring, and fixed in the same pass -- see 0134 below: root's own
fault-supervision thread had an identical 1-tick bounded-poll pattern and
ran forever regardless of shell activity. -->

<!-- 0134 evidence: measuring 0133's fix directly (idle qemu-process %CPU
on the release profile, sitting at a shell prompt) found it unchanged at
~108-110%, not the near-0% the original plan expected. Investigation
found three more instances of the exact same 1-tick bounded-`ipc_receive`
polling anti-pattern USR-022/0133 fixed for serial-driver, all with
nothing else to interleave with in their loop bodies so a plain
unbounded, genuinely blocking `ipc_receive()` (no `notification_bind`
needed -- these only ever wait on ONE thing, a real IPC message) was
sufficient: root's fault-supervision thread (`drain_fault_reports()`,
`src/user/include/sys/root_graph.hh` -- a default-argument change
preserves the bounded 1-tick wait for its OTHER three call sites, which
genuinely must not block indefinitely because they interleave with
other polling in the same loop; only `supervision_thread_entry()`'s own
dedicated forever-loop, which does nothing else, now passes
`no_timeout`), virtio-driver's steady-state service loop, and
vfs-server's steady-state service loop (both `src/user/drivers/virtio/
main.cc` and `src/user/servers/vfs/main.cc` -- unlike virtio's own
completion-wait spin, which must keep draining its IRQ notification and
is unaffected). console-server's two service loops were already
correctly blocking (no change needed). Applying all three did not move
idle %CPU either -- conclusion: that metric is dominated by QEMU/TCG's
own per-vCPU emulation overhead for this machine's `-smp 4` GICv3 +
`virtualization=on` configuration, not by guest-level busy-polling; a
`CPUS=1` isolation test to confirm by scaling was attempted but the
kernel requires exactly 4 CPUs and halts otherwise, so this remains an
inference from four independent fixes producing zero measurable change,
not a direct measurement. The original bug (serial-driver's poll making
the shell laggy) remains conclusively fixed regardless -- verified
directly, not via this metric, in 0133's certification race test and the
dramatic contrast observed while bisecting it: an otherwise-identical
build with only serial-driver/shell reverted to the old polling code
took over 3 minutes of real time to reach a boot marker the fixed build
reached in ~15 seconds, under comparable host load, because a guest that
busy-polls competes for scarce host CPU time against QEMU's other vCPU
threads in a way a genuinely blocked thread does not. Verified: all
three profiles compile clean; certification `[ACCEPTANCE] result=PASS
failures=0`; `make smoke` passed all three profiles, including the guest
profile's `restart ok` marker, which directly exercises the changed
`drain_fault_reports()` restart-on-fault path (root_supervisor_role
detecting and restarting a crashed role) -- about as direct a functional
test of that specific change as exists in this suite. One `make smoke`
run hit a 45s profile timeout under a host load spike (peaked ~6.4/8
cores, a second interactive session having started) with zero markers
seen despite the qemu process still alive and slowly accumulating CPU
(confirmed via `/proc/pid/stat` tick deltas, not deadlocked); the very
next profile in the same run, booting the identical release image and
services, passed cleanly, and a standalone re-run of just the missed
profile with SMOKE_TIMEOUT=90 passed cleanly too -- host-timing noise,
not a regression, consistent with everything else observed this
session about this host's variable load. -->

<!-- 0135 evidence: two follow-ups to 0133/0134, one a real fix and one a
disproven hypothesis recorded so it is not re-attempted blindly.

(a) FIXED -- `fork()` did not re-establish the child's own mapping of the
shared VFS transfer frame. `ensure_vfs_mapped()` (`src/user/lib/libc/io.cc`)
caches "already mapped" in process memory, and `process_fork`'s eager
address-space copy duplicates frame-backed mappings as PRIVATE copies (see
`control_operation::process_fork`'s own ABI note), so a child inherited the
flag as true, skipped `map_frame`, and then exchanged VFS requests through a
page that is no longer the frame `vfs-server` reads. Symptom was an `open()`
with `O_CREAT` intermittently failing inside a forked pipeline stage, after
which the stage wrote to inherited stdout instead of the pipe temp file and
the parent's `waitpid()` never returned -- i.e. an interactive shell that
stopped responding permanently after `cat foo | cat`. Fixed by clearing the
cache in the child branch of `fork()` so its first VFS call re-runs
`map_frame` for real. Isolation-tested: with the fix, 4 of 5 runs of a
scripted pipeline-then-keystroke session recovered fully where nearly every
run had previously wedged.

(b) NOT A DEFECT, hypothesis withdrawn -- the residual multi-second
keystroke stalls on this host are NOT caused by this kernel's scheduler and
should not be "fixed" by touching it. The theory was that because nothing in
the shipping boot graph ever calls `scheduling_configure` (only the
certification suite does), every thread keeps `scheduling::initialize()`'s
priority 128 / 1-tick budget / 1-tick period, making `next_runnable()` a
strict round-robin in which an interrupt-woken thread cannot preempt a
merely-eligible background one. Three separate remedies were implemented and
measured: a priority boost for the serial-driver/console-server/shell chain
installed via `thread_suspend`+`scheduling_configure`+`thread_resume`; the
same boost via a new non-destructive priority-only operation added
specifically to avoid `quiesce_user_thread()`'s destructive teardown; and
raising the default budget/period granularity from 1/1 to 16/16. All three
made interactive responsiveness measurably WORSE, up to near-total
unresponsiveness, and all three were reverted -- the tree carries none of
them.

The measurement the theory rested on (~650ms between a keystroke's UART
interrupt waking the serial driver and that thread's next scheduled run) is
invalid: it was taken with kernel `printk` tracing active, and `printk` goes
out the same PL011 the entire measured I/O path contends for -- one such run
emitted 98,380 log lines. That is a severe observer effect on exactly the
quantity being measured.

A controlled A/B settled it: the shell profile's scripted session was run
repeatedly against the pristine pre-change tree and the current one,
interleaved, recording `/proc/loadavg` per run. The untouched baseline
stalled and timed out at least as often as the fixed tree (baseline 15
answered / 39 timed out; fixed 40 answered / 32 timed out, across 4 runs of
18 commands each). The stalls are pre-existing behaviour of this system
under host contention -- this host ran at load ~2.4-4.0 of 8 cores with an
unrelated browser and editor session live -- not a regression, and not
something the scheduler changes above improved. Anyone revisiting
interactive latency should start by reproducing on an idle, dedicated host
with NO kernel tracing on the console UART; without that, the measurement
cannot distinguish this kernel from QEMU/TCG vCPU scheduling on a loaded
machine.

Consequently `tools/verification/smoke.sh`'s new post-pipeline keystroke
check REPORTS rather than gates. Two stricter forms were tried and both
measured as flaky on this host: a per-keystroke latency ceiling (failed on
the pristine baseline too), then a liveness gate (did any of several
keystrokes get answered) -- back-to-back runs of the identical image
alternated between answering in ~950ms and answering nothing in 20s, and one
such run additionally lost `shell ready` plus every guest-profile marker that
had passed minutes earlier, i.e. the whole VM was starved. Gating on that
would make `make smoke` fail roughly half the time for reasons outside this
kernel. The check therefore prints `N/6 answered` with latencies and does not
fail the suite; it should be promoted to a hard gate once it can be run on an
idle, dedicated host, where "0 answered" becomes unambiguous (a real
regression to the pre-0133 busy-poll shows up as 0-answered on EVERY run
rather than intermittently). -->

<!-- 0136 evidence: FIXED -- a lost PL011 RX interrupt permanently killed
console input after a burst of typed characters. Reported as "zephyr shell
hang after repeat inputting s"; the reporter's own hypothesis (an input
buffer problem) is what pointed at the FIFO boundary and found it.

Root cause, in serial-driver's `drain_rx()` (`src/user/drivers/serial`):
UARTICR was written AFTER the drain loop. A byte arriving in the window
between the final `try_getc()` (which saw RXFE and returned false) and that
ICR write left the FIFO non-empty while ICR wiped the latch that byte had
just set. The PL011 asserts RX when the FIFO level REACHES its trigger, not
while it sits at or above it, so with a byte already resident the level can
never re-cross from below: no further RX interrupt is ever delivered, and
the driver sits in `ipc_receive()` forever. Fixed by clearing RXIC BEFORE
each drain pass and looping until a pass ends with the FIFO genuinely
empty, so anything arriving during or after a pass keeps its latch.

Scope of the symptom explains why it looked like several different bugs:
the interactive shell and any hosted guest both read through this one
driver, so both died together, and downstream everything simply blocked --
console-server's stdin thread on the driver, domain-manager inside its
`serve` operation on console-server (instrumented: `serve` never returns),
root's guest loop on domain-manager. Nothing crashed, which is why no
failure marker ever fired.

Verified with `tools/verification/guest_input_burst.sh` (added): the guest
repro failed 3/3 before the fix and passed 5/5 after, and the same burst
aimed at the host shell passed 2/2. BURST=64 ROUNDS=1 does NOT reproduce --
the race needs a sustained burst to land a byte in that window, which is
why it survived earlier testing.

Three hypotheses were pursued and disproven before this one; each is
recorded because re-following them would cost the same time again:
  - Not root abandoning the guest. Instrumented root to print `serve`'s
    return value; it never printed.
  - Not the interrupt storm detector. Instrumented record_delivery()'s
    storm branch; it never fires during the repro. The ~64 echoed
    characters is serial-driver's 64-byte RX ring draining, which
    coincidentally equals storm_threshold (64). That coincidence was
    actively misleading. (A genuine latent defect was noticed while
    checking: `stormed` is latched and cleared only by bind(), while
    acknowledge() refuses to unmask while it is set, so a line that ever
    does storm stays masked for the remaining uptime. A timer-tick re-arm
    was written and reverted -- it fixes nothing observable here and
    deserves its own evidence.)
  - Not vPL011 losing an RX interrupt across an IMSC mask window. Adding
    re-injection on RXIM unmask plus refilling the one-byte RX holding
    register on DR read changed nothing; reverted.

Found by inspection while landing 0133 and unrelated to the above:
serial-driver's `read_byte_wait` defers by leaving the caller's reply
capability held across its next `ipc_receive()`, but `install_reply()`
overwrites a thread's single reply slot unconditionally, so another request
on that shared endpoint while a read is deferred would orphan the deferred
caller. Not observed in practice and not the cause of this bug. Fixed
separately in 0137. -->

<!-- 0137 evidence: FIXED -- the deferred-reply clobber left open by 0136.

Defect: serial-driver served write, write_byte, read_byte and
read_byte_wait from ONE thread on ONE endpoint, and read_byte_wait parks
(replying later, out of the interrupt path). A parked reply lives in that
thread's single `thread::reply` slot, which `install_reply()` (ipc.hh)
overwrites unconditionally on every incoming call. So any write reaching
the driver while a read was parked destroyed the reader's reply capability:
console-server's stdin thread would stay `blocked_reply` forever, and with
it every console reader in the system. Permanent, silent, no failure marker
-- the same shape as 0136 but a different mechanism.

The exposure was real, not theoretical: console-server's own write path and
the virtio driver's bring-up diagnostics are both serial-driver clients
independent of the reader.

Fix: split the driver in two along the TX/RX line.
  - New `serial_rx_endpoint` (root_graph.hh) carries only read_byte and
    read_byte_wait; write/write_byte stay on `serial_service_endpoint`.
  - serial-driver spawns a second thread under `serial_rx_role` that owns
    the RX half entirely: the bound interrupt notification, the RX ring,
    `drain_rx()`, and both read operations. The main thread serves writes
    and never touches the ring. A write can therefore no longer land on the
    thread holding a parked reply.
  - Root mints the RX endpoint to console-server ALONE, whose stdin thread
    is a single thread that blocks in `ipc_call`. At most one RX request is
    ever outstanding, so one reply slot is always sufficient. Both
    invariants are properties of the capability graph, and are documented at
    `serial_rx_endpoint` rather than left implicit in driver code -- minting
    that endpoint to a second client would silently reintroduce the hang.

One non-obvious consequence, and the one bug hit while landing this:
`thread_create` gives each sibling thread its OWN address space (only the
cspace is shared -- see `create_user_thread()`'s teardown comment). The RX
thread therefore did not inherit the main thread's UART mapping and faulted
on its first drain, which presented as a booting system whose shell echoed
nothing (`user fault delivered thread=10 cpu=3`, exactly one, at the first
keystroke). `map_uart()` now takes the target space and the RX thread maps
the device into its own; `memory::map()` supports this directly, keying
mapping records on (space, address) and rejecting only a repeat of the same
pair, and `maximum_mappings_per_frame` is 8.

Verified: `make smoke` PASS on all three profiles (service graph, shell,
guest vPL011 + restart); `guest_input_burst.sh` BURST=256 ROUNDS=6 PASS;
interactive shell echo confirmed byte-for-byte on a scripted session.

Certification: PASS. `[ACCEPTANCE] suite=root-only boot=root-only
result=PASS failures=0 failure_mask=0 transport=PASS`, with
`kernel_lifetime_invariants result=PASS`, reproduced on two consecutive
runs on an idle host.

Getting there took separating three failures from one verdict, which is
worth recording because two of the three were the host and one was not
knowable until the host was quiet:

  - `capability_transfer_revoke_race` and `ipc_capability_batch` failed
    once at host load 21, in a run that took 524s of guest time against a
    normal ~7s -- roughly 50x starved. Both pass on a quiet host. Race
    tests are what degrades first under that, so this was load.

  - The wall-clock gates (`ipc_latency_bound`, `scheduler_latency_bounds`,
    which `kernel_lifetime_invariants` rolls up) were the ambiguous one.
    `ipc_latency` max_ticks against a 620000 limit, by host load: 1900311
    at 9.5, 1436549 at 21, 823726 at 1.8 -- over the limit even at what
    looked like idle, which is why this was recorded as UNRESOLVED rather
    than written off, and why the kernel changes in the bound-notification
    commit were queued for bisection against it. On a genuinely quiet host
    (load 0.84 and 0.61) it lands at 441997 and 167597 -- 0.7x and 0.27x
    of the limit. The gate is fine and the kernel is cleared.

The lesson for the next person reading a FAIL here: load average 1.8 was
NOT quiet enough to trust this metric on this host. An unrelated multi-core
build had been running for most of the session, and a decaying 1-minute
average during a lull still measured a machine whose caches and DVFS state
had not settled. Two consecutive runs at sustained sub-1.0 load are what
made the numbers stable, and a 4.4x spread between the highest and lowest
"idle" measurement is the reason a single sample here proves nothing. -->

<!-- (continues 0137) Scope note, kept because it is what narrowed the
search while the latency result above was still open, and it stays true
for any future certification failure: `CONFIG_SELFTEST` replaces init's
main() with the harness, and `src/user/tests/certification/main.cc` never
includes root_graph.hh, nor touches vfs-server or native::text -- so
0138's changes cannot reach this image at all. Only the kernel-side
changes (notification_bind, the interrupt dispatch_result split,
signal_notification, the ipc_receive lifecycle locking) were ever
candidates, and the idle-host PASS above cleared them.

Not verified by an on-demand repro of the original failure, and that is a
deliberate limitation rather than an oversight: with the current process set
no second writer is active while a reader is parked (the shell writes only
in response to input, and the virtio diagnostics are boot-only), so the race
cannot be provoked from userspace without adding a process that exists only
to provoke it. The fix is structural -- the clobbering call can no longer
reach the parked thread -- and smoke, the burst regression, the scripted
shell session and certification all confirm nothing else moved. A latent
hang that cannot be triggered on demand is still worth closing; it is
exactly the class 0136 turned out to belong to.

Still open, unchanged from 0136: `stormed` (interrupt.hh) is a latch cleared
only by `bind()`, while `acknowledge()` refuses to unmask while it is set,
so a line that ever storms stays masked for the remaining uptime. -->

<!-- 0138 evidence: boot reliability. Found while regression-testing 0137,
and pre-existing -- every rate below was measured on BOTH the working tree
and a pristine worktree at HEAD, run alternately on the same host.

## The silent boot stall (fixed, measured)

Symptom: roughly 2-in-5 boots stop dead after the block driver's bring-up
diagnostics and print nothing further, forever. Only reproducible with a
fifo on qemu's stdin (an idle interactive terminal); with `</dev/null` it
went 8/8 clean, which is why `make smoke` sees it and a casual `make run`
does not. It is the reason smoke has looked flaky: a stalled profile
reports every marker MISSING at once, which reads like a broken assertion
and is actually a boot that never happened.

Two independent causes, one fixed:

1. FIXED -- `vfs-server`'s `map_shared_frames()` mapped its two root-minted
   frames with a ONE-SHOT `map_frame`, while every sibling bring-up path
   (serial-driver's `map_uart()`, the virtio driver's `map_mmio()` and
   `bind_irq()`) wraps the identical call in `native::retry` against the
   identical documented hazard: root mints into a child's cspace only after
   `process_create` returns, so the child can and does get there first. VFS
   lost that race, its map returned not_found, and it answered by signalling
   `failure_badge` -- which root's readiness loop turned into a silent exit.
   Now retried, matching its siblings.

2. OPEN -- a control-plane role sometimes never signals ready at all.
   Root's new diagnostics (below) caught it directly: `root: readiness
   stalled ready=0x1f7 want=0x1ff` (missing bit 3, the domain role),
   `ready=0x1ef` (bit 4, supervisor), `ready=0x1fe` (bit 0, process). It is
   not one specific role, and `root: role FAILED` also appears, meaning a
   role took a bring-up failure path. Note `bin/control-plane`'s ONLY
   failure path is `!valid(policy_for(role)) || ready == 0`, which depends
   on nothing but the `role` argument -- so a role reaching it at all is
   evidence that the value `main` received was not the one
   `process_create`/`thread_create` was given. A role receiving another
   role's value would equally explain a ready bit that never arrives (two
   roles signalling one badge). That hypothesis is NOT yet confirmed and
   the argument-passing path has not been audited; it is recorded as the
   strongest lead, not as a diagnosis.

Measured, same harness both trees, 14 boots each: baseline 6 stalls,
with the VFS fix 1. Two smaller samples inside that total agreed (3/8 and
3/6 baseline; 0/8 and 1/6 fixed).

## Root's readiness loop had no diagnostics (fixed)

None of this was findable before, and that was the real defect: root's
readiness wait is unbounded, and each of its five failure exits returned a
bare status code. A role that failed and a role that merely never answered
produced byte-identical output -- nothing. Root now reports, once and
without changing control flow, which badges are missing (`ready=` /
`want=`), and reports before each failure exit.

Deliberately routed through serial-driver's endpoint rather than
console-server's, since console-server is one of the roles being reported
on; root already holds `serial_service_endpoint`. `native::text::packed()`
was added for it -- the existing `text::write()` sends one IPC per
character, and the first captured report came out shuffled character-by-
character with the virtio driver's concurrent probe output, which cost real
time to untangle. Packed chunking drops interleaving granularity from 1
byte to 23.

The stall threshold is ~30s of loop iterations, far past a healthy boot
(~1s), because a threshold near normal boot time would report a merely
loaded host as stalled -- boot has been measured over 10s here under
concurrent load.

## `vfs absent` with a working disk (open here, FIXED in 0140)

Separately intermittent, and unrelated to the stall above: with a disk
attached and the block driver demonstrably healthy in the same boot
(`virtio: sector round trip PASS`, capacity reported, and root's own
`block-service verified`), `verify_vfs()` reports absent on roughly half of
boots -- 4/8 on the working tree, 3/5 of completed baseline boots. This is
what `make smoke`'s `vfs verified` marker intermittently trips on.

One hypothesis was tested and REJECTED: that VFS's `ext2::mount` races
root's mint of its block-service endpoint, the same shape as the map race
fixed above. Gating the mount on a bounded retry of `block_operation::info`
until the endpoint answers changed the rate not at all (4/8 before, 4/8
after), so the endpoint is reachable and the driver is fully probed by the
time VFS mounts. That attempt was reverted rather than kept, since keeping
an unvalidated change would have implied a fix that was not there. The
remaining candidates -- the shared payload frame being used by more than
one client at a time, or the probe rather than the mount reporting absent
-- are untested.

The second of those two was right, and it was the CLIENT, not VFS: see
0140. Worth noting how close the rejected hypothesis was to the real one
-- both are the same mint-ordering race, but on opposite sides of the
call. Testing the server side and finding no change is what eventually
pointed at the client. -->



<!-- 0139 evidence: one fix, one hardening, and one root cause narrowed but
NOT closed. Recorded honestly per item because two of the three open
issues from 0138 are still open.

## Interrupt storm recovery (FIXED)

`stormed` was a one-way latch: record_delivery() sets it past
storm_threshold and masks the line, acknowledge() refuses to unmask while
it is set, and only bind() ever cleared it. No driver re-binds, so any
line that ever crossed the threshold stayed masked for the remaining
uptime. A single burst of legitimate traffic -- a fast typist on the
console, a busy disk -- permanently killed the device. Containment was
implemented; recovery was not.

Now recovered by `interrupt::recover_stormed(now)`, swept once per timer
tick from CPU 0, which clears the latch and unmasks any line whose
detection window has fully elapsed. Re-storming is intended: a line that
really is stuck asserting trips again after another storm_threshold
deliveries, so the steady state is a ceiling of roughly storm_threshold
per storm_window_ticks rather than a permanent kill.

Two things worth keeping, both learned by getting them wrong first:

  - The re-arm CANNOT live in acknowledge(), which is the obvious place
    and was the first attempt. While `stormed` holds the line masked at
    the GIC nothing is delivered, so the owner has nothing left to
    acknowledge and the recovery path can never run. Nothing inside the
    interrupt path can break that cycle -- the mask being recovered from
    is what suppresses the event that would trigger recovery. Only an
    external clock can. The interrupt lifecycle test caught this
    immediately once it asserted recovery rather than only containment.
  - The sweep walks `registry[]`, not the `dynamic_interrupts` pool.
    record_delivery() and dispatch() set `stormed` on any registered
    interrupt whichever storage it lives in, so keying recovery off one
    pool leaves the others latched -- which is exactly how the second
    attempt failed, on an interrupt the test owns directly. Bounded by a
    new `registry_bound` high-water mark so the per-tick cost is a handful
    of loads rather than a 1020-slot walk.

`irq_storm_containment` previously passed whether or not recovery worked,
which is how the latch survived. The test now asserts both halves: still
contained one tick into the window, re-armed and genuinely usable again
(a fresh record_delivery + acknowledge round trip) once it has elapsed.
Reported as `[TEST] name=irq_storm_recovery result=PASS window=100
rearmed=1`.

## publish_translation_tables() (hardening, NOT a fix)

Added `dsb ishst` after the last descriptor store in initialize() and
clone(). The release store that publishes a thread as runnable orders
those writes for other PEs' ordinary loads, but a translation table walk
is a separate observer and the architecture requires an explicit DSB.

It was added while chasing the boot stall below and it did NOT fix it --
the stall rate was unchanged across 20 boots. It is kept because the
barrier is required independently of that symptom, and it is labelled that
way at the call site so nothing credits it with a fix it did not make.

## The silent boot stall (still OPEN, cause narrowed)

Now measured at 2/16 with everything above applied. 0138's VFS one-shot
map was one real cause; this is the other, and it is a kernel SMP defect,
not a userspace ordering race.

Signature, captured by temporarily relaxing VERBOSE_DIAGNOSTICS' `depends
on BUILD_DEBUG` to get esr/far/pc in a release graph boot (the Kconfig
edit was reverted):

    user fault delivered thread=8 cpu=3 esr=2000000 far=0 pc=20000000

esr EC = 0 ("unknown reason" -- what executing a zero word raises),
far = 0, pc = 0x20000000 = the image entry point, and ALWAYS on a CPU
other than the one that built the address space. So the entry page is
mapped -- an unmapped one would raise EC 0x20/0x21, an instruction abort
-- and contains zeros rather than the ELF image. A thread begins executing
before its image contents are visible on its own CPU.

The amplification is what makes it a total silent stall rather than one
dead thread, and it is worth understanding separately:
`drain_fault_reports()` replies `fault_disposition::terminate` to every
fault, including from threads root does not manage. When the casualty is a
serial-driver thread the console dies, and then EVERY writer in the system
blocks forever inside its own report call -- root, console-server, the
block driver. One captured boot's entire userspace output is the single
character `v`: the virtio driver got one byte of "virtio: ..." through and
blocked on the second. That also explains why root's own 0138 diagnostics
sometimes print nothing at all: they route through serial-driver, which is
precisely what is dead.

Ruled out by measurement, not assumption:
  - Not the missing page-table DSB (added anyway, above; no rate change).
  - Not `asid::refresh()` failing and activate() silently returning
    without installing TTBR0 -- that early return looks alarming and is
    dead code, since refresh() delegates to allocate(), which always
    succeeds. (Still a latent hazard: if it ever did fail, a thread would
    run in the PREVIOUS address space installed on that CPU, which is an
    isolation violation rather than a crash. Worth closing separately.)
  - Not the loader's cache maintenance being absent: synchronize_
    instruction_cache() does `dc cvau` over each page, `dsb ish`, `ic
    ialluis`, `dsb ish; isb`, and activate() additionally does a local
    `ic iallu` after installing TTBR0.

The kernel's own comments already state that this area is a bounded
bootstrap approximation which "must be replaced with generation-tracked
residency and targeted cross-CPU synchronization". This is evidence that
the approximation actually fires in normal operation, roughly one boot in
eight. Fixing it properly is that replacement, which is a design task, not
a patch -- and a speculative barrier or ASID change here risks converting a
crash into a silent isolation violation, which is why none was attempted.

Diagnostics added along the way and kept, since none of the above was
observable before them: root reports the missing ready badges and then
probes each missing role's health with a BOUNDED call (a plain one blocks
forever against a dead role, hanging root inside its own diagnostic), so
"role alive, badge lost" and "role not answering" are distinguishable --
every case observed is the latter, `st=-7`, timed_out. And serial-driver
now reports its own bring-up failures by writing the PL011 directly rather
than over IPC, because it is the one process whose failure takes the
reporting path down with it.

## `vfs absent` (still OPEN here, FIXED in 0140)

Not investigated further in this pass. -->

<!-- 0140 evidence: FIXED -- `vfs absent` on a healthy system with a
mounted disk, the last of 0138's open items.

Defect, in libc's `ensure_vfs_mapped()` (`src/user/lib/libc/io.cc`), the
client side rather than the server side that 0138 tested and cleared:

root's `spawn()` calls `process_create`, which makes the child runnable
immediately, and only THEN mints the child's capabilities -- args frame,
stdout, stdin, vfs endpoint, and `native::vfs_frame` LAST of the five. A
spawned program therefore reliably reaches its first `open()` before that
final mint lands. `ensure_vfs_mapped()` was a one-shot `map_frame` that
set `attempted = true` before trying, so losing that race latched
`mapped = false` permanently: every subsequent open/read/write in the
process returned -1 for the rest of its life.

It surfaced as `vfs absent` because `verify_vfs()` maps `bin/vfs-probe`'s
exit status 10 -- "`open(\"/etc/motd\")` failed" -- onto absent, which is
the same answer a machine legitimately booted with no disk gives. A
correct "no disk attached" signal and a lost capability race were
therefore indistinguishable from outside, which is most of why this took
two passes to find.

Fixed by making the single attempt a bounded retry over not_found/denied,
the same convention every server's own bring-up already follows -- this
was the one client-side copy that did not. `busy` counts as mapped, since
fork()'s child clears these flags and re-maps (its address-space clone
does not carry frame-backed mappings) and a second map_frame at the same
address reports busy rather than success. The latch is still set
afterwards, so a program with genuinely no VFS capability pays the retry
once rather than on every call.

One ordering property is load-bearing and worth not breaking: because
`vfs_frame` is minted LAST, waiting for it also establishes that the
earlier mints have landed -- including `vfs_endpoint`, whose absence would
otherwise fail `open()`'s `ipc_call` for exactly the same reason and would
need its own retry. Reordering spawn()'s mints would silently reintroduce
that second race.

Measured, fresh disk image per boot, same harness as 0138's measurement:
19 of 19 completed boots report `vfs verified`, 0 absent -- against 4 of 8
absent before. `make smoke` PASS on all three profiles, and certification
`[ACCEPTANCE] result=PASS failures=0 failure_mask=0 transport=PASS` with
both wall-clock latency gates passing too (`ipc_latency` max_ticks=224124
against the 620000 limit, on a host at load 2.9).

Still open, and now the only one left from 0138/0139: the silent boot
stall, which accounts for the 2 non-completing boots in the sample above.
Its cause is characterised in 0139 and is a kernel SMP defect, not this. -->


<!-- 0141 evidence: the boot stall, still OPEN. This entry exists so the
next attempt starts where this one stopped instead of re-deriving it. Real
diagnostics landed; no fix did, and nothing speculative was kept.

## The one decisive new fact: it needs parallel vCPUs

Same build, same host, back to back:

    -accel tcg,thread=single    0 stalls / 16 boots
    default (MTTCG)             5-7 stalls / 16 boots

That is the cleanest signal in this whole investigation. The failure
requires genuinely concurrent CPUs, so it is a real SMP race (in the
kernel, or in QEMU's MTTCG fidelity -- see the open question at the end),
and NOT host load, not scheduling luck, and not anything in the userspace
graph.

## What actually faults

Release builds previously logged only "user fault delivered thread=N
cpu=M", which cannot distinguish an undefined instruction from a
translation fault. The warn now carries pc, esr, spsr, sp and the fault
count. With that, every captured stall is the same shape:

    pc=20000000 esr=2000000  spsr=0 sp=20050000 faults=1
    pc=20000000 esr=82000007 spsr=0 sp=20050000 faults=1

pc is the image entry, sp is exactly stack_top, spsr is the initial
PSTATE, faults=1. So this is a brand-new thread failing on its FIRST
instruction, before it has executed anything, always on a CPU other than
the one that created it. Two exception classes appear:

  - esr=0x2000000  -> EC 0x00, undefined instruction: the fetch returned
    something that is not an instruction.
  - esr=0x82000007 -> EC 0x20, instruction abort, IFSC 0x07 = translation
    fault at level 3: the entry page is not mapped in the space the CPU is
    walking.

Both are "the new address space is not correctly visible to the remote CPU
at first fetch", from opposite directions.

## Levers pulled, and rejected on measurement

None of these changed the rate beyond noise, so none was kept. They are
listed because each looks obviously right and re-trying them costs the
same day again:

  - dsb ishst after the last page-table store (kept anyway, as 0139
    records, because the barrier is required independently -- but it is
    NOT the fix).
  - tlbi vmalle1is instead of tlbi aside1is in activate(), i.e. a full
    stage-1 flush rather than by-ASID, in case of ASID aliasing: 1/16
    against 2/16. Noise. Reverted.
  - ic ialluis + dsb ish instead of ic iallu + dsb nsh in activate(), i.e.
    broadcasting the instruction-cache invalidate: 1/16. Noise. Reverted.

Also ruled out by inspection rather than measurement:

  - Interrupts nesting inside load_user() and swapping TTBR0 between
    activate() and the eret. This would explain both fault shapes exactly,
    and it is wrong: boot/vectors.S never issues daifclr, so IRQ/FIQ stay
    masked for the whole handler.
  - Page tables being misaligned. memory::table_t is alignas(page_size).
  - The physical page allocator handing the same page out twice. Both
    allocate_physical_page() and allocate_resource_page() mark the bitmap
    under the allocator lock, and the general path additionally skips
    pages inside a child's delegated extent.
  - asid::refresh() failing and activate() returning without installing
    TTBR0 (which would run a thread in the previous space -- an isolation
    violation, not a crash). refresh() delegates to allocate(), which
    always succeeds, so that early return is dead code. Still worth
    deleting or converting to a panic on its own merits.

## A self-inflicted lesson worth keeping

An earlier version of this diagnostic also printed the instruction word at
the entry point, read through the image's backing page. It was genuinely
useful -- it is how the "page is mapped but holds zeros" hypothesis was
raised and then disproved -- but it dereferences process memory from
INSIDE the fault handler, so a torn-down or half-built address space
faults the kernel there, in the handler for a fault. That turned a
recoverable user fault into a deterministic total lockup: `make smoke`
went from PASS to both release profiles producing no output at all, twice
in a row, which looks exactly like a much worse regression. Removed. A
diagnostic must not be able to fault.

## Where to start next

The open question is whether this is a kernel race or a QEMU MTTCG
fidelity issue, and that is worth settling BEFORE more kernel changes,
because every architecturally-motivated fix tried above came back as
noise. The kernel's publication sequence reads correct on inspection: the
loader does dc cvau per page, dsb ish, ic ialluis, dsb ish, isb; the
tables get a dsb ishst; the thread is published with a release store;
activate() does tlbi, msr ttbr0, isb, then a local ic iallu, dsb nsh, isb
before the eret.

Settling it needs either targeted TB-invalidation instrumentation in QEMU,
or a run on real hardware. Note that KVM is not a shortcut on this host
despite it being aarch64: the machine is built with virtualization=on and
the kernel uses EL2, which an A76 cannot nest. -->

<!-- 0142 evidence: the boot stall's mechanism, IDENTIFIED. Still not
fixed, but no longer a mystery: it is ASID recycling under a live address
space, and the fix is a bounded piece of work rather than an open question.

## The measurement that found it

0141 ended without a mechanism because every diagnostic so far described
the corpse. The one that worked reads three things at fault time, all safe
from inside a fault handler -- a system register and two loads from the
address_space object in kernel BSS, never a walk through the faulting
process's own memory (0141 records why that distinction is not optional):

  - `ttbr`  -- TTBR0_EL1 as actually installed on the faulting CPU
  - `want`  -- the root + ASID the space currently believes it has
  - `l3e`   -- the L3 descriptor for the image entry page

Captured on a stalled boot:

    pc=20000000 esr=2000000 faults=1
      ttbr=a00004016e000 want=b00004016e000 l3e=405467c3

Same table root (0x4016e000). Valid L3 descriptor (0x...7c3: valid, page,
AF set, inner-shareable, EL0-readable) for exactly the page that could not
be fetched. And the ASID installed in TTBR0 is 0xa while the space claims
0xb.

So the tables are correct, the mapping is correct, the barriers are
correct -- which is why every cache and TLB fix in 0141 came back as
noise. The CPU is running the right page tables under the WRONG ASID.

## Why that is fatal

TLB entries are tagged by ASID. A CPU running a space under a stale tag
does its lookups against a tag that asid::allocate() is free to have
handed to a DIFFERENT space, so two live spaces alias one tag and the
first one's cached translations answer for the second. That produces
exactly the two fault shapes 0141 catalogued: an undefined instruction
when the aliased entry resolves to another image's bytes, and a level-3
translation fault when it resolves to a space that has nothing there.

It also explains the SMP dependency precisely. Under
`-accel tcg,thread=single` no second CPU is running a space concurrently,
so no alias can form -- 0 stalls in 16 boots, against 5-7 with MTTCG.

## Fixed here (partial, and it is NOT the fix)

activate() wrote value.asid and then read it BACK to build TTBR0 and the
TLBI operand. value.asid is shared mutable state, so a concurrent
activate()/refresh() of the same space could land in between. Both now use
`identifier`, the ASID the call actually resolved.

That closes the read-back window only. Measurement says plainly that it is
not the cause: the mismatch still appears afterwards (ttbr=0x9... against
want=0xb..., same root), so the ASID is ALSO being recycled while its
space is live. Kept because the read-back race is real on its own terms,
labelled at the call site as not explaining the symptom.

## What actually needs doing

asid::release() clears a tag's in_use bit, and asid::refresh() reallocates
whenever the tag's bit is clear or the generation moved -- neither
consults whether any CPU still has that tag installed in TTBR0.
rollover_locked() has the same shape at scale: it resets the whole in_use
mask and bumps the generation, so every live space reallocates on its next
activate while CPUs keep running the old tags.

A correct ASID allocator must not recycle a tag that is live on any CPU.
address_space::active_cpu_mask already tracks residency and is already
maintained by activate() -- it is set there and, as far as this
investigation found, never consulted. That is the missing half, and it is
what the kernel's own comment means by "generation-tracked residency and
targeted cross-CPU synchronization".

Deliberately not attempted in this pass: an ASID allocator change is
exactly the kind of edit where a wrong guess converts an intermittent
crash into a silent isolation violation -- two processes quietly sharing
translations instead of one visibly dying. It wants its own pass, with the
mismatch check above kept as the regression test, since it detects the
condition directly rather than waiting for a boot to hang. -->

<!-- 0143 evidence: the ASID allocator, hardened. The boot stall is STILL
open, and this entry is careful to separate what was proven from what was
merely hoped, because an earlier draft of it got that wrong.

## What was actually wrong with the allocator

Two real defects, both found from 0142's ttbr/want mismatch:

  - `allocations` counted LIFETIME allocations, not live ones: release()
    cleared the in_use bit and left the counter alone. The rollover
    trigger (`allocations >= capacity - 1`) therefore fired every 63
    allocations ever made, no matter how few were outstanding. With at
    most 16 live spaces against 63 usable tags, rollover should be
    unreachable in this kernel; process churn reached it in under a
    second. It is now a live count.
  - Nothing stopped a tag being recycled while a CPU still had it in
    TTBR0. rollover_locked() cleared the whole mask, and allocate() only
    consulted in_use. A tag handed to a second live space while the first
    still translates through it means two address spaces sharing one TLB
    tag -- an isolation hazard, not merely a crash. The allocator now
    tracks the tag each CPU has installed (asid::installed[], maintained
    by activate() and cleared by activate_kernel()), skips installed tags
    when allocating, preserves them across rollover, and returns no_memory
    on genuine exhaustion instead of handing out tag 1 unconditionally --
    which is what the old fallback did, aliasing it with whoever held it.

## Proven: aliasing was happening, and is now prevented

The mismatches after the change have a signature that is itself the proof:
`want` is consistently the installed tag PLUS TWO (ttbr asid 7 against
want 9; 8 against 0xa), on an identical table root. That is allocate()
stepping over the tags currently installed on other CPUs instead of
reusing one. Before the change it would have returned the same tag again.

## NOT proven: that any of this fixes the boot stall

It does not, on the evidence. Two A/B rounds, same host, back to back,
release profile, 16 boots each:

    round 1   without: 7/16    with: 2/16
    round 2   without: 1/16    with: 3/16
    combined  without: 8/32    with: 5/32

Round 1 alone looks like a fix and round 2 alone looks like a regression;
together they are noise, and the between-round swing (7/16 then 1/16 on
the SAME build) is larger than the effect being measured. So the change is
kept as allocator hardening on its own merits, and explicitly not as the
fix. Anyone measuring this needs paired A/B rounds on an otherwise idle
host -- a single sample of 16 proves nothing here, which this
investigation has now demonstrated twice.

## The sharper lead, for the next pass

The mismatch means a live address_space object has acquired a NEW tag
while a CPU still runs the old one, on the same table root -- so the
object is being re-initialized underneath a running thread. One capture
caught it mid-rebuild: `l3e=0`, the entry page's descriptor not yet
written.

`process_exec` (thread/scheduler.hh) is the one path that does exactly
that, and says so: it calls activate_kernel(), then initialize() on the
CURRENT space, then activate() -- with its own comment noting "exec is the
first caller that rebuilds a live space rather than retiring it". The
faults match a post-exec first instruction exactly: pc == image entry, sp
== stack_top, faults=1.

That is where to look next, and it is a much narrower target than "an SMP
race somewhere in address-space publication". -->

<!-- 0144 evidence: two theories killed outright, and the culprit path
identified. The boot stall is still OPEN, but the search space is now
small.

The fault warn gained two more fields, both safe BSS reads:
`inits` (how many times this address_space object has been BUILT) and
`rollovers` (asid::rollovers). They settled two questions immediately.

## rollovers=0 -- the ASID rollover chain is not the cause

Every captured stall reports rollovers=0. Rollover never happens during
boot at all, so 0143's entire line of reasoning -- lifetime-counted
allocations forcing spurious rollovers, rollover reassigning live tags --
cannot be what produces the stall. That work stands as allocator
correctness (it does prevent a real aliasing condition, per 0143's +2
evidence) and is now positively excluded as the mechanism here.

A genuine ASID leak was found and fixed while checking this:
initialize() is called on LIVE spaces, not only fresh ones, and allocated
into a fresh handle while discarding value.asid -- leaking one tag per
exec. It now releases the previous tag first. Also correctness, also not
the fix; with rollovers=0 the leak never got far enough to matter.

## inits=2 -- the space is rebuilt in place, and exec is the path

Most captures report inits=2: the address_space object was built twice.
Only one path rebuilds a live space rather than retiring it, and it says
so in its own comment -- process_exec() in thread/scheduler.hh, which
calls activate_kernel(), then initialize() on the CURRENT space, then
activate(). Boot reaches it through verify_fork_exec().

That matches the fault shape exactly: after exec a thread restarts at the
new image's entry with a fresh stack, which is precisely
pc=<image entry>, sp=stack_top, faults=1.

With ttbr == want and a valid l3e in the same captures, translation is
correct by then. So what remains is narrow: after exec rebuilds a space,
the CPU sometimes fetches non-instruction bytes for the NEW image at
virtual addresses it was very recently executing the OLD image from.

## The one loose end in that story

Not every capture is exec: one reported inits=1, a freshly created space
failing the same way. So either there is a second path into the same
condition, or the fresh-creation case shares whatever the rebuild case
exposes. That is the first thing to settle next, because it decides
whether the fix belongs in process_exec() specifically or in the
image-publication path generally.

## Measurement discipline, restated

Rates in this entry are not quoted, deliberately. 0143 established that a
16-boot sample cannot resolve an effect this size on this host -- the
same build measured 7/16 and then 1/16 in consecutive rounds. Any claimed
fix here needs paired A/B rounds on an idle machine, and `inits`/`ttbr`/
`want`/`l3e` are better signals than the stall rate anyway, because they
detect the condition directly instead of waiting for a boot to hang. -->

<!-- 0145 evidence: a use-after-free of physical pages, FIXED on the reap
path -- and the same condition is now directly detectable, which is what
the remaining work needs.

## The signature

The user-fault warn now carries, in addition to 0144's fields, the
physical page the faulting PC resolves to and whether the ALLOCATOR still
considers that page handed out:

    pc=20000000 esr=2000000 ttbr=8...15d000 want=8...15d000
      l3e=4051e7c3 inits=1 word=0 phys=4051e000 held=0

Read that carefully: ttbr equals want, so translation is consistent; l3e
is a valid page descriptor; phys is a real page. And held=0 -- the page a
LIVE address space currently maps is one the allocator believes is free.
word=0 follows directly: whoever allocated it next zeroed it, underneath
the mapping still pointing at it.

That is a use-after-free of physical memory, and it became the dominant
signature once 0143/0144's ASID work removed the noise on top of it.

## Fixed: reap freed a bundle whose thread was still executing

`terminated` and "not running anywhere" are different facts, and
reap_user_bundle() conflated them. It checked `load_state(target) ==
terminated` and quiesced every SIBLING, but never waited on the target
itself. A thread reaches terminated the instant its state is stored --
from thread_exit on its own CPU still inside the syscall, or from another
CPU entirely via ipc.hh's caller termination -- and its CPU need not have
switched away yet. So reap could release the target's page tables and
physical pages, and mark its slot inactive, while that thread was still
running with the space installed in a CPU's TTBR0. The freed pages then go
to the next allocation and get zeroed.

destroy_user_bundle() has always called quiesce_user_thread() on its
target first. Reap is the one teardown path that did not.

The wait is now await_not_executing(), extracted from
quiesce_user_thread()'s tail rather than reusing the whole function, and
that distinction was measured, not assumed: quiesce_user_thread()
publishes `suspended` before waiting, which is correct when forcing a live
thread to stop and WRONG for an already-terminated one being reaped --
overwriting `terminated` loses the marker the reaper needs if any later
step returns early, leaving a child nothing can ever collect. Using it
made the stall markedly worse (9/16 against a 2-4/16 baseline) because the
failure mode moved from a rare race to a permanently unreapable probe.
await_not_executing() waits without touching state.

## The rollover test was asserting the bug

`asid_rollover_reuse` allocated and immediately released `capacity + 4`
times and expected a rollover. That only ever worked because the allocator
counted LIFETIME allocations (0143): release() cleared the tag's in_use
bit but left the counter raised, so 68 allocate/release pairs "exhausted"
a pool that never held more than one tag at once. With the counter now
tracking live allocations -- which is correct -- that loop rightly never
rolls over, and the test failed the whole bootstrap self-test.

Rewritten to exhaust the pool genuinely, by HOLDING the handles it takes.
Only the last one needs releasing afterwards: every tag taken before the
rollover was freed by the rollover itself, and release() ignores their
stale generation. It deliberately does not build an array of handles --
a zero-initialised one compiles to a memset call this freestanding kernel
does not link, which is its own small lesson about tests in this
environment.

Certification is green with it: `asid_rollover_reuse result=PASS
generation=2 rollovers=1`, and `[ACCEPTANCE] result=PASS failures=0
failure_mask=0 transport=PASS`.

## Still open

The boot stall persists, and `held=0` still appears, so at least one more
path frees a page that is still mapped. Candidates not yet examined:
memory::reclaim_task_memory() on the exec path, and the interaction of
root's release_child(), which calls process_destroy AFTER process_reap has
already cleared the same bundle.

The useful change is that this no longer needs the stall to reproduce.
`held=0` on a valid l3e detects the condition directly, at the moment it
bites, and it is worth turning into an assertion rather than a printout
once the remaining producer is found. -->

<!-- 0146 evidence: detach-before-free, and a permanent barrier against
freeing a page a live address space still maps. The boot stall is still
open; what changed is that this class of corruption can no longer happen
silently.

## Naming the releaser

0145 could detect a page freed while still mapped but not say who freed
it, and every candidate site had already been audited and found guarded --
so inspection had run out. release_physical_page() now records the return
address of each free in a small ring (128 entries; a per-page table would
be 64 KiB of permanent BSS to answer a question that only ever concerns
the last few frees), plus a caller-set "context" identifying the address
space a rebuild is attributed to.

Resolving those addresses with `llvm-addr2line -e out/.../zilch.elf` named
`release_stack_backing` inlined into `arch::space::initialize`, and
`thread::clear_user_bundle`. Note the first symbolisation attempt was
misleading: `release_image_backing`/`release_stack_backing` are inlined
into several callers, so a return address can land in a symbol that is not
the real caller. The `context` field exists precisely to disambiguate that
and it is what showed these frees were NOT attributable to a rebuild.

## Fixed: initialize() and release() freed pages their own tables still mapped

`initialize()` released a space's image and stack pages and only cleared
block 0's L3 table afterwards. Between those two steps the descriptors
pointed at pages already handed back to the allocator -- live translations
into memory anything could be given next. `release()` had the same order.

This is the exact hazard `release_dynamic_tables()` already documents for
L3 tables ("Clear the L2 entry BEFORE handing the page back... The page is
only safe to release once nothing can reach it"), just never applied to
the pages themselves. Both now clear the table first.

## Added: the allocator refuses to free a mapped page

`release_physical_page()` now consults a probe -- installed by the thread
layer at boot, since the answer needs the thread tables and the allocator
cannot include them -- asking whether any LIVE address space still maps
the page. If one does, the free is refused and reported, leaking the page.

That is a containment barrier, not a diagnostic. Freeing such a page
zeroes it (release_physical_page scrubs on free) and hands it to the next
allocation, silently destroying a running process's code or stack. A
bounded leak plus a named culprit is strictly better than that.

It earned its place immediately: armed, it fired on the certification
profile against the old initialize() ordering above -- sixteen stack pages
per occurrence -- which is how that ordering bug was found at all. With
the ordering fixed it fires zero times across certification and 16 release
boots, so it is precise rather than merely loud.

"Live" deliberately excludes inactive and terminated slots: a slot
mid-teardown is exactly the one legitimately handing pages back, and
counting it as live would fail every normal free. The probe walks L3
descriptors rather than the image_backing/stack_backing arrays, because
descriptors are what a CPU can still translate through -- a page dropped
from the bookkeeping but left in a table is the dangerous case, not the
reverse.

## Where the stall stands

Still open, and the surviving fault signature is now clean enough to be
worth stating exactly:

    pc=20000000 esr=82000007 ttbr=8...15b000 want=8...15b000
      l3e=0 inits=1 freedby=0 held=0

ASID consistent, space built exactly ONCE, nothing ever freed that page --
and the L3 entry for the image entry is simply zero. So this last case is
not a use-after-free and not ASID aliasing: a thread is executing while
its space's descriptors are absent. Either the thread became runnable
before initialize() finished publishing them, or the fault is attributed
to the wrong thread (current_user_thread[cpu] stale across a switch).
Distinguishing those two is the next step, and it is a much smaller
question than any this investigation started with. -->

<!-- 0147 evidence: THE BOOT STALL, FIXED. Two concurrent creators could
claim the same thread slot, and the second rebuilt the first one's address
space underneath its running thread.

## The defect

find_free_user_slot() searched for a slot whose state was `inactive` with
a plain load, and returned it. There was no claim. So two creators running
on different CPUs could both observe slot N free and both build into it:

  - CPU A: create_user_bundle -> initialize_user -> initialize() builds the
    space, loads the image, publishes `ready`. Its thread starts running.
  - CPU B: had already selected the same slot, and its initialize() now
    clears that space's L3 table and reloads a different image -- under a
    thread that is executing from it.

The running thread then faults at its image entry with an empty L3.

Concurrent creators are not exotic here, which is why boot hit it: root
calls process_create for each role on CPU 0, while console-server and
serial-driver each call thread_create for their own second thread from
their own CPUs, all inside the same few tens of milliseconds of graph
bring-up.

## Why it took so long to see

Every earlier theory was consistent with the evidence and wrong, and each
one had to be excluded by measurement rather than argument -- the record is
in 0141 through 0146. What finally identified it was making the address
space report how many times it had been BUILT, and then fixing that
counter: `initializations` was incremented after the teardown it describes,
so a rebuild in progress read inits=1 and looked exactly like a first
build. Moving the increment ahead of the mutations turned every captured
fault into inits=2 -- two builds of one space -- with byspace == self,
i.e. the space rebuilding itself. That is unambiguous.

The SMP-only behaviour follows directly and had been the strongest clue
all along (0141: 0 stalls in 16 boots under `-accel tcg,thread=single`
against 5-7/16 with MTTCG) -- with one vCPU there is never a second
creator to race.

## The fix

The slot is now claimed atomically, moving it `inactive -> suspended` with
a compare-exchange, so exactly one creator can win it. `suspended` is the
right marker: it is not `inactive`, so no other claimer takes it; it is not
runnable, so the scheduler skips a half-built thread; and every existing
failure path already stores `inactive`, which releases the claim for free.

initialize_user() had to stop storing `inactive`. It set the slot back to
inactive as its "known starting state", which would have re-opened the
claim for the whole of construction -- the exact window being closed. It
now stores `suspended`, and the creator still publishes `ready` when done.

## Measured

Release service graph, fifo on stdin (the configuration that reproduces
it), on the same host that was producing 2-7 stalls per 16 boots
throughout this investigation:

    40 boots (16 + 24):  0 stalls, 0 user faults, 0 barrier firings

`make smoke` PASS on all three profiles. Certification `failures=0
failure_mask=0 transport=PASS` with zero release-of-mapped-page firings and
a normal ~4s guest time; the wall-clock latency gates alone still trip at
host load 6 (694251 against a 620000 limit) and pass on a quiet host, which
is the pattern 0137 documents. -->

<!-- 0148 evidence: the investigation's scaffolding removed, and the whole
series re-verified end to end.

Finding the boot stall (0141-0147) needed a lot of instrumentation, and
most of it had no business staying in a shipping kernel: a fourteen-field
fault line, a 128-entry release-history ring, and helpers that existed
only to answer questions now answered. Kept vs removed, deliberately:

KEPT, because each pays for itself independently of that investigation:
  - The page allocator's refusal to free a page a live address space still
    maps, with its thread-layer probe (maps_physical_page) and the
    release_context attribution in its report. This is a safety barrier,
    not a diagnostic -- it converts a silent use-after-free of physical
    memory into a bounded leak plus a named culprit, and it found the
    detach-before-free ordering bug the moment it was armed (0146).
  - installed_root()/expected_root() in the fault line. Two cheap reads --
    one system register, two BSS loads -- that answer "was this thread
    running on the tables it was supposed to be?", which is the question
    that took longest to get evidence for.
  - initialization_count(). One field, and the field that actually cracked
    it: a space built more than once means it was rebuilt rather than
    created, which is what separates "a new process failed to start" from
    "a live process was rebuilt underneath itself".
  - esr/spsr/pc/sp/faults in the fault line. A release build previously
    reported only thread and cpu, which cannot tell an undefined
    instruction from a translation fault.

REMOVED as pure scaffolding: the release-history ring and its lookups
(the barrier reports the site at the moment of the free, so a history to
consult after the fact is redundant), physical_page_allocated(),
entry_descriptor(), rollover_count(), mapped_word() and
mapped_physical(), plus the l3e/rollovers/word/phys/held/freedby/byspace/
self fields. mapped_word() in particular read process memory from the
fault handler, and while the final version was guarded by a descriptor
check, an earlier one was not and locked the kernel up (0141) -- not a
thing to leave lying around once it has served its purpose.

Re-verified after the trim, on the same host:
  - 24 release boots with a fifo on stdin: 0 stalls, 0 faults, 0 barrier
    firings
  - make smoke PASS on all three profiles, twice consecutively
  - certification [ACCEPTANCE] result=PASS failures=0 failure_mask=0
    transport=PASS on a quiet host, 2.5s guest time, 0 barrier firings
  - guest_input_burst.sh BURST=256 ROUNDS=6 PASS
  - scripted interactive shell: both commands echoed and executed

That closes every issue opened in this series. -->

<!-- 0149 evidence: USR-024 closed. Driver restart coverage, and the kernel
defect that was actually blocking it.

## The premise was wrong

USR-024 deferred restart coverage because "every holder of a capability
into a restarted driver would need a mid-flight re-mint". That is not what
the capability graph does. A client holds a capability to the service
ENDPOINT OBJECT, which root created and owns; destroying the driver task
does not touch it. restart_role() already documented exactly this for the
five control-plane roles -- "root's copy of the endpoint object survives
the old task's destruction untouched, so every OTHER role's capability
into it stays valid automatically" -- and the same reasoning was simply
never applied to the two drivers.

So restart is destroy + create + re-mint into the fresh child's empty
cspace, with root's originals as the source. restart_service() does that,
parameterised per service by a `service_restart` descriptor (serial, block
and VFS all have one).

The create half -- device_frame_create, interrupt_create -- is deliberately
NOT replayed: both reject a second live object for the same physical page
or IRQ. That exclusivity is usually described as the thing making restart
hard; it is the opposite. Because root holds those originals and they
outlive the driver, there is nothing to recreate.

## What was actually blocking it

A kernel defect, not the graph. An interrupt whose owning task was
destroyed mid-delivery kept `active` set, with nobody left to acknowledge
it. interrupt::bind() refuses while `active` is set -- correctly, since
rebinding under a live owner would strand the acknowledge it owes -- so
the line became permanently unbindable and the driver could never take it
back. A driver was unrestartable precisely when it most needed restarting:
after crashing while servicing its own interrupt.

bind() now takes over a line whose bound notification no longer resolves,
which is how "the owner is provably gone" is detectable. It deactivates at
the controller first, so the fresh owner starts from a clean GIC state
rather than inheriting a half-serviced one, and still refuses while the
previous notification does resolve.

Covered by `irq_orphan_takeover`: live owner -> busy, orphaned -> rebound
with `active` cleared.

## Proof, in the profile smoke gates

  - `block-restart ok` destroys the block driver, waits for it to report
    ready again, and re-runs the same write/read sector round trip. It
    asserts the restarted driver reports the SAME result as the pre-restart
    check rather than merely "not failed" -- otherwise, on a machine with no
    disk, absent would pass unconditionally and a restart that broke a
    working device would slip through on the profile that has one.
  - `serial-restart ok` restarts serial-driver, and is written THROUGH the
    driver that was just restarted. The marker appearing is the proof. This
    is the only shape available for a service that owns the reporting path
    it would otherwise need to report on: a failed restart ends the boot log
    there, which smoke's missing-marker check catches, rather than needing a
    FAILED line the dead console could not print.
  - `guest alive via vpl011` still passes after both, so the whole console
    chain -- domain-manager to console-server to serial-driver -- recovers.

Measured 3/3 with a real disk attached and 3/3 without, plus `make smoke`
PASS on all three profiles and certification `[ACCEPTANCE] result=PASS
failures=0 failure_mask=0 transport=PASS` with zero
release-of-mapped-page firings.

VFS has a descriptor on the same path but is not exercised; VFS restart is
not what this item names, and adding a third restart to the one profile
that also hosts a guest buys little for the boot time it costs. -->

<!-- 0150 evidence: DEV-001/002/003 closed. Revocation severs device
authority, not just the capability naming it.

The device-assignment mechanism was largely already there -- root-gated
device_frame_create with per-physical-page exclusivity, root-gated
interrupt_create, capability_mint to delegate, capability_revoke to
withdraw -- and irq_ownership_delegation already proved the delegation
half. What none of it did was make revocation mean anything to the
hardware.

revoke_descendants() cleared the cspace slot and stopped. So after root
revoked a driver's capabilities:

  - the interrupt object stayed bound to that driver's notification and
    stayed unmasked, and the device kept firing into a driver that no
    longer held a capability to it;
  - the MMIO page stayed mapped in that driver's address space, so it
    could no longer name the device but could still drive it.

The name was revoked; the authority was not. For a capability system that
is not a gap in a device feature, it is a soundness bug.

Revocation now runs a hook per revoked capability. Interrupts are masked at
the controller, any in-flight delivery is dropped, and the line is unbound,
so re-delegation starts clean through bind(). Device frames have every
mapping dropped across whatever spaces hold them, via a new
memory::release_frame_mappings() -- the inverse of unmap_all(), which drops
every mapping of one space rather than every mapping of one frame.

Three structural points, each load-bearing:

  - The hook fires only after every cspace lock is released. The device
    teardown takes memory_mapping (rank 40) while the sweep holds cspace
    (rank 50), so calling it inline would invert the order. The revoked
    objects are collected during the sweep and handled afterwards, which is
    the same two-phase shape revoke already used for its own mark/sweep.
  - cspace.hh does not know which object types own hardware, and should
    not. It calls an installed hook; the type filter lives in the thread
    layer, which is the one place that can see both interrupt.hh and
    memory/manager.hh (each of those includes cspace.hh, so neither can
    reach the other).
  - Frame unmapping is restricted to DEVICE frames. An ordinary shared
    frame legitimately has several holders -- VFS's client buffer, the
    block driver's payload page -- and revoking one holder's capability
    must not unmap it from the others. A device frame is exclusivity-
    checked to a single live owner, so reclaiming it means exactly this.

irq_ownership_delegation now asserts the severed state (masked, not
active, unbound) rather than only that the capability lookup fails --
the old assertion passed whether or not the line was still live, which is
how this survived.

Verified: certification [ACCEPTANCE] result=PASS failures=0
failure_mask=0 transport=PASS with all five irq_* tests passing, and
make smoke PASS on all three profiles. -->

<!-- 0151 evidence: DEV-005 closed, DEV-004 documented. Assignment
rollback, plus two corrections that testing it forced.

## Rollback

Provisioning creates the task and then mints its capabilities. A failure
anywhere after the create used to just return, leaving a half-provisioned
task alive: a stranded thread slot, cspace and memory resource, and
holding whatever device capabilities did land -- so the device read as
busy to the next attempt while nothing was driving it.

restart_service() now destroys the fresh task on any provisioning failure.
One operation rather than an undo list, because destroying the task frees
its whole cspace, and since revocation severs device authority as well as
naming (0150) the frame and line come back genuinely reclaimable.

## Two things testing it exposed

Neither was the feature being built, and both were real:

  - The opening process_destroy had to become best-effort. It establishes
    a precondition -- nothing occupying these selectors -- rather than
    performing an operation whose failure matters, and "there was nothing
    there" is that precondition already met. Treating it as an error made
    recovery impossible exactly after a rollback, which is when recovery is
    the whole point. A destroy that fails for a real reason still stops the
    restart, because process_create then fails busy on what it did not
    free.
  - Every path that frees a space's pages now identifies itself to the
    allocator's release-of-mapped-page barrier. Without that, a space
    handing back its OWN pages looked exactly like one taking them from a
    live holder, and the barrier refused: 23 leaked pages per boot that
    forks. The state alone cannot distinguish the two cases --
    quiesce_user_thread() publishes `suspended` before teardown and a
    claimed-but-unbuilt slot is `suspended` too -- so the releaser says who
    it is instead. The miss was fork's clone(), which opens by releasing
    the destination's image and stack (the ones initialize_user() had just
    built, and which are loaded only to be discarded) before copying the
    parent's in.

That second one is worth dwelling on: the barrier added in 0146 was doing
its job precisely, and the leak was the cost of it not being told who was
calling. A safety check that cannot tell self-teardown from theft will
refuse honest work, and refusing honest work quietly is its own defect.

## DEV-004: documented, not implemented

Recorded honestly as partial. Two devices are assignable. The PL011's
reset requirement (mask RX at IMSC, drain the FIFO, clear ICR) is now half
performed by the kernel -- release_ownership() masks, drops any in-flight
delivery and unbinds -- and half by the incoming owner's own bring-up. The
virtio-mmio block device's requirement (write 0 to Status, which per spec
resets it and abandons the virtqueue) is not performed on revocation at
all; a restarted driver re-initialises from whatever the previous owner
left, which works only because it rewrites Status during bring-up. Neither
device is reset BY THE KERNEL on reassignment, so freedom from state
leakage rests on each incoming driver re-initialising. That is exactly
what DEV-017 asks to be proven, and why it stays open.

Verified: make smoke PASS on all three profiles with `assign-rollback ok`
gated; guest profile 5/5 with zero barrier firings; certification
[ACCEPTANCE] result=PASS failures=0 failure_mask=0 transport=PASS with
zero firings; 12 release boots, 0 stalls. -->

<!-- 0152 evidence: DEV-006 closed. SMMU discovery, and why the rest of
section 9.2 stops there rather than being built unexercised.

## Discovery

The device tree is searched for a node whose `compatible` names
`arm,smmu-v3`, its `reg` window retained past the parse, and IDR0/IDR1/IDR5
read out of it. Matched on the binding string rather than the node name --
the name carries the base address and so differs between machines.

Verified both ways on the machine tools/run/run.sh boots:

    (default)        smmu: absent (no arm,smmu-v3 node)
    ZILCH_SMMU=1     smmu: arm,smmu-v3 at 9050000 idr0=d44101b idr1=2730010
                           idr5=74 stage1=1 stage2=1 sidbits=16
                           translation=off

S1P and S2P both set, SIDSIZE=16 -- exactly QEMU's SMMUv3. Identification
registers only; no write touches the device. ZILCH_SMMU=1 adds
iommu=smmuv3 to the dumpdtb invocation as well as the run, so the blob the
kernel parses describes the machine it is actually running on; getting that
wrong would have the kernel discover a device the machine does not have.

One bug worth recording, because the first version passed a machine that
HAS an SMMU as having none: `reg` was consumed only if `compatible` had
already been seen. Property order within a DTB node is not guaranteed. The
reg is now captured provisionally per depth and committed at end_node, when
both halves are known in either order.

The SMMU is kernel-owned rather than delegated, unlike every other device
in this system. It is what makes that delegation safe -- it is what stops
an assigned device's DMA from reaching memory its owner was never given --
so a userspace driver holding it could remove its own containment.

## Why 9.2 stops here

Not effort. On QEMU's virt machine the SMMUv3 fronts the PCIe root complex
and nothing else: `iommu-map` belongs to pcie@10000000, and not one of the
thirty-two virtio_mmio@... nodes carries an `iommus` property. Confirmed by
dumping the DTB of the machine this project boots. This kernel's only real
device is virtio-mmio, which bypasses the SMMU entirely.

So a stream table, per-domain translation context, invalidation path or
fault handler written now would translate for no device, and every
assertion about it would be vacuous. The release evidence section of this
very checklist already rules that out -- "no mandatory feature relies on a
model, mock, fixture, or hard-coded resource pool" -- and building it
anyway would convert twelve open items into twelve items that look closed
and prove nothing, which is worse than leaving them open.

Reaching the translation path needs a DMA-capable device behind the SMMU.
On this machine that means PCIe, which means a root-complex driver: ECAM
enumeration, BAR assignment, MSI through the ITS. That subsystem does not
exist in this kernel and is not itself listed as a requirement anywhere in
this document -- which is a gap in the requirements, not just in the code,
and is worth fixing before section 9.2 is attempted again.

DEV-006 is closed because discovery is the part that can be exercised
against the real device. DEV-007 carries the shared blocker; DEV-008..018
point at it.

Verified: make smoke PASS on all three profiles; certification
failures=0 failure_mask=0 transport=PASS with zero barrier firings; both
SMMU-present and SMMU-absent boots reach `graph ready`. -->

<!-- 0153 evidence: 7 of the 12 hypervisor gates recorded as complete. This
is composition and bookkeeping, NOT new proof, and the distinction matters
enough to state plainly.

## What was actually found

Every one of HYP-001 through HYP-047 was already complete, each with its
own recorded evidence, and HYP-EXEC-GATE with it. Sections 8.1 through 8.6
are at 47 of 47. The twelve gate lines beneath them were all still
unchecked -- they have no criteria of their own, they compose their
sections, and nobody had gone back to tick them as the sections finished.

So the work here was to verify the composition rather than trust the gate
names: count each section's requirements, confirm none is open or partial,
and re-run the certification suite to confirm the hypervisor evidence still
holds today rather than only when it was first recorded.

It does. The suite executes the hypervisor tests in the certification
profile (CONFIG_HYPERVISOR_SELFTEST and CONFIG_GUEST_TEST_ARM64 are both
set there), and all of them pass: vm/vcpu create, destroy, stale, reuse and
parent-busy; dynamic lifecycle; real single-vCPU and real SMP execution;
real multi-VM isolation; negative fuzz; the three control-model cases;
virtual timer lifecycle; domain manager API, guest load and guest run. 144
PASS overall, with the only failures being the wall-clock latency gates
that need a quiet host (0137).

## What this is not

Checking a gate whose sub-items are ticked is exactly the trap this
document warns about elsewhere -- a gate that looks closed and proves
nothing. Two guards against that here: each gate line now names the
section it composes AND the certification tests that exercise it, so the
claim is auditable rather than asserted; and where a section is not
complete the gate stays open even though its tests pass, which is the case
for the userspace VMM gate (7.5 has 5 open, 2 partial) and the stress/soak
gate (12.3 has 2 open and 4 partial, 12.4 has 5 open). Passing tests are
not the same as a complete section, and the five open gates say so
explicitly.

Two gates are open for reasons no amount of work in this environment
changes: device assignment and SMMU, blocked by the platform (DEV-007 --
nothing sits behind QEMU virt's SMMUv3 that this kernel drives), and real
hardware ARM64 certification, which needs the kernel booted on physical
hardware when every result in this document comes from QEMU.

## Status of the two 1.0 gates

Hypervisor: 7 of 12. Kernel: 7 of 11, unchanged -- its userspace
control-plane gate composes section 7, which 7.5 and 7.6 keep open, and its
verification/documentation/real-hardware gates are the same three classes
of open work as above. -->

<!-- 0154 evidence: USR-026/027/028/033 closed, and a pre-existing shell
defect found while verifying.

## 7.5, mostly bookkeeping again

USR-026, USR-027 and USR-028 were all already implemented and never
updated, the same pattern as DEV-001/002. VM creation goes through
capability selectors only (`vm.create(vm_selector, vcpu_selector, ...)`,
and `map_frame` takes an authorized frame capability per HYP-015); the
guest ELF is parsed and loaded entirely at PL3 in domain-manager, with
per-page W^X derived from section flags; and the guest's memory and device
layout comes from a userspace manifest, with the kernel supplying only the
mechanism. Each is now recorded with where the code lives and which
certification test covers it.

USR-030/031 (Linux/BSD guests) stay open with the prerequisites written
out, because their size should not be mistaken: PSCI for secondary CPU
bringup, a guest-visible GIC distributor/redistributor (HYP-025's
controller is bounded and vCPU-resident with no distributor emulation), a
constructed device tree, and a root filesystem path. Zephyr boots because
the manifest hands it exactly a PL011, the virtual timer and one forwarded
IRQ.

## USR-033: exit-status monitoring, and a live badge collision

A role that EXITS is not a role that faulted, and only the latter was
noticed. The supervision thread watches the fault endpoint, which a clean
thread_exit never touches, and the readiness loop checked the failure badge
and nothing else -- so a service that returned from main (console-server's
`stop` path does exactly that) simply vanished: endpoint unanswered, no
restart, no report.

`handle_role_exits()` now reads the per-role exit badges from the
notification the readiness loop already polls, clears that role's stale
readiness bit, and restarts through the same bounded admission control as a
fault restart. Driven from that loop rather than the supervision thread
deliberately -- that thread blocks indefinitely on the fault endpoint,
which is what stopped it waking every tick, and giving it a second thing to
watch would mean returning to polling.

Fixing it surfaced a live collision. `control_plane_exit_badge()` started
at bit 8, which is `vfs_service_ready_badge` -- a process-role exit and a
VFS readiness signal were the same bit in one accumulated word. It had gone
unnoticed precisely because nothing read exit badges. Moving them revealed
a second, undocumented constraint the hard way: at bit 16 they landed in
the upper half of the word, which the certification harness treats as an
unexpected-badge failure (`badges & 0xffff0000`), and
userspace_control_plane_graph failed outright. That contract lived only in
the harness. Exit badges now occupy bits 9-13, and four static_asserts keep
the readiness, exit and failure classes disjoint AND inside the low 16
bits, so neither mistake can recur silently.

Gated by `exit-restart ok`: a real `stop` to the device role, with the
restarted instance required to come back and answer a health RPC. 3/3.

## Found while verifying, NOT introduced: the shell wedges after one pipeline

`make smoke`'s `redirect + pipeline output` check began failing at 2 of 3
occurrences. Investigating it found something smoke cannot see: the shell
runs the FIRST `cat file | cat` correctly and then never returns to a
prompt. Subsequent commands are echoed but never execute -- five
consecutive pipelines produce exactly one line of output.

Bisected to be pre-existing: the same probe against f9ccc32, before any of
this session's lifecycle work, also completes exactly one pipeline. Smoke
never caught it because smoke runs exactly one pipeline, which is also why
its marker count of 2-vs-3 is a timing artifact of that single run rather
than the real defect. The real defect is that a second pipeline never runs
at all.

Not fixed here, and deliberately not rolled into this change. Recorded as
its own finding so it is not mistaken for fallout from the badge or
lifecycle work. -->

<!-- 0155 evidence: FIXED -- 0154's shell wedge. The global authority lock
was unfair, and this kernel cannot afford an unfair lock.

## What it actually was

Not a shell bug, not a fork bug, not a capability-leak bug. Every kernel
lock in this tree went through one `spin_lock()` (cspace.hh), and it was a
bare test-and-set: exchange 1 into the word, spin on a relaxed load until
it reads 0, repeat. No queue, no ticket, no fairness of any kind.

That is survivable in a kernel where callers block. This kernel has no
blocking wait -- `process_wait` and `notification_poll` both return `busy`
and require the caller to poll again, and their own ABI comments explain
why adding a blocking scheduler state was out of scope. So a parent in
`waitpid()` polls, every poll resolves a capability, and every capability
resolution takes the global authority lock. The polling CPU re-acquires a
line it already owns exclusively and wins essentially every race; under
QEMU's MTTCG, where a vCPU runs a long translation block before yielding,
it wins nearly all of them. The parent starved exactly the child it was
waiting for, plus the vfs-server that child was mid-call to.

The symptom's shape follows from that directly, including the parts that
had looked contradictory: the child DID run and DID produce its output
(the prompt came back, `tok` printed), it simply could never finish
exiting. And the run-to-run variance -- one, two, or zero commands before
the wedge, with `vfs absent` reappearing in the worst runs -- is what
starvation looks like, not what an exhausted fixed-size resource looks
like. Two resource-exhaustion theories were tested and both were wrong:
yielding in the poll loop, and deleting the reaped child's capability.

## The fix

`spin_lock`/`spin_unlock` are now a ticket lock. Two 16-bit counters are
packed into the single word every lock here already is -- `halves[0]` is
the ticket being served, `halves[1]` the next to hand out -- so no lock
changes size or layout, and equal halves still mean unlocked, which is
what lets every zero-initialized lock word in the tree stand unchanged
(endpoint.hh:36 and cspace.hh:281 were both audited). Indexed as an array
rather than by bit position, so it is endianness-neutral; nothing reads
the word as a whole u32. Tickets wrap at 65536, far above
`maximum_cpu_count` waiters, and the wait is an equality test on the
wrapped value, so rollover is harmless.

FIFO is the whole point: a poller takes a fresh ticket behind every
existing waiter each time round, which bounds every other CPU's wait.

## Two corrections to the record

`thread_yield` (control op 54) was added while chasing this and does NOT
fix this symptom -- measured, no change. A poller with nothing else
runnable on its CPU is simply re-selected and goes straight back to the
lock; yielding cannot help cross-CPU lock contention. It is kept because
the gap it fills is real on its own terms (fork()'s own comment named the
missing syscall, and its workaround -- placing the child on another CPU --
is still in scheduler.hh), but it is not the fix and should not be cited
as one.

The other theory tried here -- that waitpid had to delete the reaped
child's capability before fork could reuse the selector -- was reverted
rather than kept. It was wrong twice over: it changed nothing when
measured, and reap_user_bundle already deletes that exact capability
itself (scheduler.hh's delete_capability_locked call), so the extra
control call was redundant as well as misattributed.

And `make smoke`'s `redirect + pipeline output` gate is weaker than it
looks. It runs `cat /tmp/smoke.txt | cat`, where a producer stage whose
stdout redirection silently failed writes the same text to the console
that a working pipeline does -- the gate cannot tell success from that
failure. A `| wc` probe that CAN tell them apart is now in smoke.sh,
reported rather than gated for the reason 0156 records.
(Note for the next investigator: `bin/wc` parses no flags at all, so
`wc -l` there opens `-l` as a filename and prints nothing. That is wc's
limitation, not a pipeline defect; a probe using it will mislead.)

Verified: 5/5 consecutive `cat file` against a build that previously
completed one, measured identically under burst and character-paced input.
`make smoke`: PASS, all gates, including `redirect + pipeline output
(seen 3 times)` -- which had been intermittently reporting 2. Kernel
certification: `suite=root-only result=PASS failures=0 failure_mask=0
transport=PASS`, with 4-CPU root fuzz clean at 4096 operations per CPU,
which is the contended-lock case that matters for this change.

Two hazards a ticket lock adds over test-and-set were checked rather than
assumed, and both are clear here. A thread preempted mid-spin would stall
everyone behind its ticket, but the IRQ path (arch.cc) only reprograms the
timer and returns -- it never switches threads, so a spinning thread keeps
its CPU. And GCC emitted no outline-atomics helpers for the 16-bit
operations, which would not have linked freestanding; the build is the
proof. -->

<!-- 0156 evidence: OPEN. Two console/pipeline defects separated out from
0155's lock fix, with the measurements that bound them, and one correction
to 0135.

## What 0155 did NOT fix

0155 removed a real, system-wide starvation source and raised the ceiling
from ONE completed external command to all six a probe asks for. It did
not make the console path sound, and the remaining symptoms are not the
same bug wearing a different mask. They are recorded here separately so
the next investigator does not re-derive the eliminations.

The honest number is a distribution, not a figure. Five runs of the same
six-command probe against the same binary completed 0, 3, 5, 5 and 6 of
them. The 0 run is its own shape: the shell never printed even its FIRST
prompt, though `shell ready` had been reported -- i.e. the console path
can be dead from boot, not only after N commands, and that is the same
phenomenon the ungated keystroke check reports as 0/6. So "dies after five
commands" was an artifact of small samples; what is actually true is that
the console path fails intermittently at any point, including immediately.

This variance predates 0155 (`vfs absent` shows up in these logs across
the session, and 0135 recorded the keystroke flakiness long before), and
the evidence that 0155 is not its cause is that certification passes
root-only with 4-CPU root fuzz clean, `make smoke` passed three
consecutive times, and the ceiling moved up rather than down.

## Eliminated, with the experiment that eliminated each

Worth writing down because each of these looked plausible and cost a run:

  - NOT fork/child-lifetime exhaustion. Instrumenting sh's run_pipeline
    with per-branch markers showed five clean `F`(fork) `W`(wait) `k`(reaped)
    cycles and then a stall that never reaches `F` at all -- the shell dies
    in read_line(), before forking. A builtin-only sequence (`echo`, no
    fork anywhere) stalls the same way, which rules the child path out
    entirely.
  - NOT burst/FIFO overrun. Byte-at-a-time input paced at 60ms/char and a
    whole-line burst write stall after the SAME number of commands. The
    pacing that tools/verification/smoke.sh's comment credits for avoiding
    this makes no difference here.
  - NOT the interrupt storm detector. Raising `storm_threshold` from 64 to
    1000000 changed nothing.
  - NOT a kernel fault, and not lock-related. No [WARN]/[ERR] of any kind
    is emitted between entering init.elf and the stall.

## What the instrumented driver actually shows

Tracing rx_main()'s own states (`[D]` deferred, `<N` notification woke,
`R>` replied from the drain path, `[h]` served straight from the ring)
gives a clean `[D]<NR>` per byte in steady state, and at the stall:

    ... o[D]        <- deferred, and no `<N` ever follows

So the RX thread parks with a reply owed and is never woken again while
input keeps being typed. Giving its ipc_receive a 50-tick (0.5s) timeout
and dumping PL011 registers on expiry produced only TWO timeouts across
~70 seconds of stall -- so the IPC receive timeout itself also stops
firing, which is a second defect and the reason the driver cannot poll
its own way out. The one register dump obtained reads:

    ris=0020 mis=0000 imsc=0010 fr=0090

ris bit 5 is TXIS (expected -- the driver transmits and never clears it,
harmlessly, since TXIM is masked). RXIS is clear, mis is zero, and fr bit
4 is RXFE=1: the UART has no received byte and no pending RX interrupt.
The typed character is not sitting undrained in the FIFO -- it never
reached the device. That points upstream of the driver (host/QEMU chardev
flow control, or the console-server stdin thread) rather than at
drain_rx()'s ICR ordering, which 0136 already fixed and which this
evidence does not implicate.

Not fixed. Deliberately not patched around either: adding a poll to
rx_main would paper over whichever of the two mechanisms is real.

## NOT a second defect: `| wc` was a red herring

Worth recording as a wrong turn, because the evidence for it looked
strong. `cat /tmp/smoke.txt | wc` produced nothing and left the FOLLOWING
pipeline producing nothing too; reordering smoke's commands so `| wc` ran
third made the previously-passing `redirect + pipeline output` gate drop
to 2 of 3 occurrences, and restoring the order made it pass at 3 again.
That looked like ordered damage caused by one command.

It is not. Two isolating runs settle it:

  - `wc /tmp/a.txt` standalone works and prints `1 1 6 /tmp/a.txt`, so
    the image, its role binding, the `%7ld` printf and VFS file reads are
    all sound.
  - `echo`, then `cat /tmp/a.txt | cat` (works, "hello" and a prompt),
    then `wc /tmp/.pipe0` -- which wedges. And in the other run, `echo`,
    then `wc /tmp/a.txt` (works), then `cat /tmp/a.txt | wc` -- wedges.

In both, the THIRD command failed, whichever command it was. The
all-`cat` probes fail at position three or four with no wc anywhere. So
position is what predicts the failure, not the command, and there is one
remaining defect here rather than two: commands stop working after a
variable number of them, which is the console/RX stall above.

The `| wc` probe stays in smoke.sh, reported and NOT gated, because the
assertion is still the right one -- it is the only thing that can tell a
working pipeline from a producer whose redirection silently failed -- but
it sits late enough in the sequence to be measuring the stall instead.
Promote it to a hard gate once the stall closes.

## Correction to 0135

0135 concluded the flaky keystroke-liveness check was host contention and
not a defect in this kernel, on the strength of an interleaved A/B against
the pre-change tree. That conclusion should not be leaned on. The readiness
loop is itself a poller on the global authority lock 0155 found to be
unfair, so the baseline it was compared against had the same starvation
source -- an A/B between two builds that share a defect cannot exonerate
it. Post-0155 runs have been observed at 6/6 answered with 25-29ms
latencies where 0/6 was previously typical, but a later run on the same
build reported 0/6 again, so this is better and still variable. It stays
ungated, now for an honest reason: the measurement is unstable, not
demonstrably external. -->

<!-- 0157 evidence: two SMP-width defects, one fixed latently and one
recorded as the reason it cannot be exercised.

## fork placed children on CPUs that may not exist

fork_user_bundle() chose its child's CPU with

    (parent.pinned_cpu + 1U) % maximum_cpu_count

maximum_cpu_count is the COMPILE-TIME maximum (4), not the number of CPUs
actually online. At any narrower width the child is pinned to a CPU that
was never brought up, where it is never scheduled and never runs -- the
parent then polls process_wait forever on a child that cannot start.

Now searches for the next ONLINE CPU (arch::smp::is_online) and falls back
to the parent's own CPU when nothing else is up, which is the only option
on a uniprocessor and is survivable there now that the authority lock is
fair (0155) and thread_yield exists.

Stated precisely, because it matters for how much this is worth: at the
width every profile here boots, all four CPUs are online, the first
candidate is `parent + 1`, and the behaviour is IDENTICAL to the old
expression. So this is a latent-correctness fix. It repairs no observed
symptom and none should be attributed to it. `make smoke` PASS and
certification PASS after it are evidence of no regression, not evidence
the fix does anything at this width.

## Why it cannot be exercised: the kernel requires full SMP width

Trying to validate the above found the harder limitation. Booting with
CPUS=1 or CPUS=2 does not reach userspace at all:

    [INFO] smp: boot CPU online
    [ERR] secondary CPU startup failed=-1

after which kernel.hh halts. Secondary startup is driven from the
compile-time maximum rather than from what firmware reports, so anything
short of four CPUs is a hard boot failure rather than a narrower
configuration. Every profile in this tree boots at exactly four, which is
why neither this nor the fork-placement bug above was ever noticed.

Not fixed here -- making SMP width configurable means reconciling
maximum_cpu_count, platform::firmware::boot_info.cpu_count and the
secondary-startup expectation, and then re-validating the pinning table,
the per-CPU timeout queues and the per-CPU tick counters against a width
they have never run at. That is real work, not a one-line bound change,
and it should not be bundled into a lock fix.

Worth knowing for readiness purposes regardless: "runs on arm64" in this
tree currently means "runs on a 4-CPU arm64", and no evidence exists for
any other width. -->

<!-- 0158 evidence: OPEN. The interrupt storm detector has never worked,
and switching it on costs more than the gate allows.

## The defect

record_delivery() timestamps its window with
platform::timer::ticks(arch::cpu::current_id()) -- whichever CPU took the
interrupt, and acknowledge()'s route_to_current_cpu() moves that around --
while recover_stormed() is always called with CPU 0's counter (arch.cc).
platform::timer::ticks() is PER-CPU and the counters genuinely diverge:
tick_count[cpu] advances by that CPU's own programmed_delta, so a CPU
idling on a long deadline jumps while a busy one steps by one.

So `now - window_start` subtracts two unrelated clocks. When CPU 0 lags,
the unsigned subtraction underflows to an enormous value that always
clears storm_window_ticks, which resets the window on essentially every
delivery and means window_count never approaches storm_threshold. The
one-second/64-interrupt window the constants describe has therefore never
been in force, and the detector is inert.

That also explains an earlier null result: raising storm_threshold from 64
to 1000000 changed nothing, because the threshold was never being reached
either way.

## Why it is not fixed here

Pinning both ends to one clock (`storm_clock()` returning
platform::timer::ticks(0U)) makes the window real -- and regresses
certification. Measured back-to-back on the same host, one run at a time
(the previous three attempts at this comparison were invalid: the
certification image never exits on its own, so three runs had been
overlapping and starving each other -- one "control" booted in 483s):

    baseline (HEAD)   acceptance at guest 2.6s   ipc_latency 367951   PASS
    with storm_clock  acceptance at guest 5.1s   ipc_latency 628661   FAIL

against limit_ticks=620000, with scheduler_latency_bounds and
kernel_lifetime_invariants failing alongside. Boot roughly doubles.

Two causes, both real. The detector starts actually suppressing: 64
interrupts per second is far below legitimate device rates (a PL011 at
115200 baud is ~11500 bytes/sec, one interrupt each), so real traffic gets
masked for up to a second at a time. And storm_clock() puts an atomic load
of CPU 0's counter on every interrupt dispatch on every CPU, which is
cross-CPU cache-line traffic on the hot path -- with the threshold raised
to 1000000 so suppression cannot fire, latency was still 999619, so the
load alone is not free either.

Fixing this properly means choosing storm_threshold from measured
interrupt rates rather than from the current placeholder, and finding a
window clock that does not serialize every dispatch through one cache
line -- arch::timer::counter() is a genuine global monotonic source and is
the obvious candidate, but it is in different units and converting it
touches the ABI's tick-based timeout contract. That is its own piece of
work.

Recorded rather than shipped. The status quo is a detector that does
nothing, which is at least honest about its behaviour once written down
here; a half-fix that doubles boot time is not an improvement. -->
