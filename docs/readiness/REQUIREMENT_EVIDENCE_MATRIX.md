# Requirement-to-evidence matrix

Status in the leading table follows
`docs/readiness/PRODUCTION_READINESS_CHECKLIST.md`. Tables under numbered
historical batch headings preserve the status at that batch and are superseded
by later consolidated rows.

| Requirement | Status | Design | Implementation | Verification/evidence | Limitation |
|---|---|---|---|---|---|
| PRD-001/002 | Complete | Product/test separation | `Makefile` build variants | ARM64 certification and release builds | None for configuration boundary |
| PRD-003 | Complete | Product/test separation | `tests/include/`, conditional guest target | release ELF/source gates | Test code excluded from release; test headers remain in repository |
| PRD-004 | Complete | Boot flow | release build | product boot log with selftests disabled | QEMU evidence retained externally |
| PRD-005 | Complete | Guest ownership | `src/user/guests/test-arm64/` | guest-boundary and production ELF gates | Certification uses temporary blob adapter |
| PRD-007..009 | Complete | ABI separation | `include/abi`, `tests/include/sys/test_abi` | production ELF gate | None |
| PRD-010..012 / DOC-014 | COMPLETE | `include/abi/sys/v1`, `tests/abi/layout.cc`, `docs/architecture/abi/ABI_COMPATIBILITY.md`, `docs/readiness/RELEASE_AND_COMPATIBILITY_POLICY.md` | `make abi-check abi-headers-check`; certification and release builds | Native ABI v1 remains intentionally C++-namespaced; a future C binding must preserve the same wire contract |
| PRD-013..017 | Complete | Hypervisor module boundaries | `src/kernel/include/sys/kernel/hypervisor/`, `tests/.../control_models.hh` | ARM64 certification/release; source gate | Header-only implementation remains |
| PRD-018 | In progress | Module rules | module files below current 500-line limit | source line-count inspection | Automated dependency rule still open |
| CAP-001..021 / CAP-GATE | COMPLETE | `docs/readiness/CSPACE_DESIGN.md`, `src/kernel/include/sys/kernel/capability/lifecycle.md` | guarded 4×64 CSpaces, generation-tagged bounded derivation tree, authority transactions, copy/mint/move/delete/two-phase revoke, reader grace periods, and four-capability call/reply transfer with complete rollback | `capability_completion_gate`, `capability_database_invariants`, 128 lifecycle cycles, 193-descendant revoke, 4,096-operation cross-CSpace fuzz, deterministic ABA, transfer/revoke and lookup/destroy races, mapping-authority revoke, four-CPU SMP fuzz, teardown, and reuse all PASS | Production capacities are explicit: 32 registered CSpaces, 256 slots each, 4,095 usable derivations, depth 64, and IPC batches of four; exhaustion fails closed |
| SCH-001..024 / SCH-GATE | COMPLETE | `docs/readiness/SCHEDULING_CONTEXT.md`, `src/kernel/include/sys/kernel/interrupt/timing.md` | fixed ten-thread/four-CPU priority scheduler, quiescent affinity migration, bounded sporadic contexts and timeout queues, IPC donation/inheritance, targeted wakes, complete state validation, and enforced telemetry limits | `scheduler_completion_gate`, `scheduler_database_invariants`, `scheduler_latency_bounds`, 21,600-period six-logical-hour soak, donation/depth tests, lifecycle races, pager/memory services, four-CPU fuzz, teardown, and reuse PASS | Ten-millisecond latency is the QEMU certification-profile bound; hardware-specific qualification remains a separate gate |
| CAP-003..005 | COMPLETE | `docs/readiness/CSPACE_DESIGN.md` | two-level radix, eight-bit guards, per-leaf occupancy bitmaps, rotating allocation hint in `capability/cspace.hh` | `guarded_cspace_scale` spans 193 slots and all four leaves; wrong-guard negative passes | CSpace capacity is intentionally bounded at 256 slots |
| CAP-020 | COMPLETE | `docs/readiness/CSPACE_DESIGN.md` | guarded cross-CSpace copy/lookup/delete and bitmap reuse | `cross_cspace_transfer_fuzz` passes 4,096 generated operations; successful and rollback batched IPC transfer pass | None for guarded cross-CSpace operation |
| CAP-016 | COMPLETE | generation-tagged object and derivation lifetime | derivation handles encode a non-wrapping generation; records with live children cannot be reused; inactive intermediate records retain exact-generation ancestry | `derivation_generation_aba`, `object_lookup_destroy_race`, `capability_transfer_revoke_race`, and four-CPU destroy/reuse acceptance PASS | Storage is intentionally bounded; exhausted generations fail closed |
| IPC-017/018/020 | COMPLETE | `docs/readiness/FAULT_IPC_PROTOCOL.md` | scheduler exception classification, public fault ABI, userspace pager policy | two real data-fault resumes plus real PL3 `udf` termination; `userspace_pager_service` and full acceptance pass | None for classified fault delivery and pager disposition |
| IPC-019 / MEM-023 | COMPLETE | `include/abi/sys/v1/fault.hh`, `docs/readiness/FAULT_IPC_PROTOCOL.md` | kernel publishes kind/syndrome/address/PC; pager consumes the frozen layout | ABI layout/header gates plus two PL3 data faults and one PL3 `udf` validate nonzero ESR and exact metadata | Architecture-specific syndrome bits remain intentionally opaque to the common ABI |
| MEM-024 | COMPLETE | `thread/scheduler.hh`, `docs/readiness/FAULT_IPC_PROTOCOL.md` | fault-page and access-type validation precedes map/resume; failure preserves pending state and reply authority | `pager_invalid_reply` plus real userspace wrong-page/read-only rejection followed by successful retry; UDF terminate path remains green | None |
| TIM-002..004 / SCH-019 | COMPLETE | `docs/readiness/TIMER_PROTOCOL.md` | per-CPU absolute virtual-timer programming, timeout-head idle deadlines, delta-based logical time, saturating deadline arithmetic | `timer_deadline_timebase`, per-CPU boot timer verification, `timeout_queue_order`, pager timeout, and full root-only acceptance | Active runnable CPUs intentionally retain the 100 Hz scheduling quantum; sporadic-server conformance remains SCH-010 |
| SCH-009/010/016 | COMPLETE | `src/kernel/include/sys/kernel/scheduling/context.hh`, `docs/readiness/SCHEDULING_CONTEXT.md` | real-time-stamped per-slice and donated-budget replenishment plus lock-hold telemetry | `sporadic_server_replenishment`, `lock_hold_measurement`, donation/timeout/pager suites, and full root-only acceptance pass | None for these requirements |
| SCH-017/020..023 | COMPLETE | `src/kernel/include/sys/kernel/interrupt/timing.hh`, `src/arch/arm64/arch.cc`, `syscall/ipc.hh` | per-CPU IRQ-masked, IRQ-service, preemption-service, cross-CPU-wake, and IPC-service maxima and sample counts | final acceptance requires nonzero samples and every maximum below the QEMU profile limit | Hardware-specific targets remain in the separate real-hardware gate |
| TST-035 | COMPLETE | `tools/release/check_section_permissions.sh` | release ELF section-flag audit | ARM64/AMD64 release builds and `binary-permissions-check` | Hardware-specific latency targets remain open |
| IPC-021/022 | COMPLETE | `docs/readiness/PAGER_FAILURE_PROTOCOL.md` | fault deadline, pager-exit reply cleanup, single pending-record bound in scheduler/control/IPC | `pager_timeout_death`, `nested_fault_bound`, userspace pager integration | Userspace service restart policy is tracked by MEM-026 |
| IPC-005/006 | COMPLETE | endpoint-to-lifecycle lock protocol | cancellation locks the resolved endpoint, locks IPC lifecycle, revalidates state and selector, removes membership, clears reply/donation state, and wakes before releasing in reverse order | `ipc_lifecycle_races`, blocked destroy/reuse, root SMP fuzz, `ipc_completion_gate`, and full four-CPU acceptance PASS | None for the bounded endpoint model |
| CAP-015 / MEM-016 | COMPLETE | authority retirement followed by object grace period | frame/resource/page-table destruction excludes map/revoke/release, frame retirement is published under the mapping lock, and authority is released before reader synchronization | `mapping_database_invariants`, `capability_database_invariants`, four-CPU lifetime acceptance, mapping-authority revoke, destroy/reuse, pager, and pressure suites PASS | Reclamation and indexes use the documented fixed-capacity production model |
| CAP-012, CAP-014..015 / MEM-019 | COMPLETE | complete process-bundle retirement transaction | target quiescence and resource retirement precede an authority transaction that revalidates control, unmaps the address space, drains the task CSpace, and revokes thread/task/address-space/scheduling-context references before reader grace periods | `object_destroy_reuse`, four-CPU certification, `kernel_lifetime_invariants`, `process_lifecycle_invariants`, and `capability_database_invariants` PASS | Authority and mapping indexes use documented fixed capacities |
| IPC-006/008 / IRQ-002/004 | IN PROGRESS | retire/revoke/quiesce/reuse lifecycle | IPC endpoint and nonblocking notification retirement are complete; IRQ registry uses atomic publication and mask-before-rebind | dynamic IPC objects, IPC completion, object destroy/reuse, IRQ delegation/ack/storm suites, and final invariants PASS | IPC portions are complete; data-driven external IRQ discovery remains |
| MEM-025/026 | COMPLETE | `docs/readiness/PAGER_FAILURE_PROTOCOL.md` | lifecycle-serialized idempotent mapping and bounded orphan termination policy | `same_page_fault_serialization`, pager exit/timeout containment, mapped OOL clients, and full memory completion PASS | Policy intentionally terminates orphaned faults rather than silently reassigning them |
| MEM-001..026 / MEM-GATE | COMPLETE | bounded DTB discovery, physical allocator/resources, generation-checked mapping database, address-space teardown, and pager protocol | generated QEMU DTB loaded at the firmware probe; fallback is rejected; `memory_completion_gate` aggregates lifecycle, mappings, revoke, attributes, delegation, extent, rollback, SMP, teardown, reuse, pager, and OOL grant tests | ARM64 certification reports `source=platform-probe`, two regions, `memory_inventory_invariants=PASS`, and final acceptance PASS | Capacities and supported ARM64 firmware/platform policies are intentionally bounded and documented |
| USR-006/009 | In progress | userspace pager batch | `src/user/servers/memory` | `userspace_pager_service` PASS | bounded resources and certification policy |
| USR-013..015 | In progress | ELF bootstrap loader | ARM64 bootstrap ELF loader | pager/client and SMP reuse runtime PASS | path-based userspace loader, TLS/auxv open |
| HYP-016 | In progress | real guest execution | arch EL2 backend and kernel vCPU run | `hypervisor_real_single_vcpu` PASS | production userspace VMM absent |
| HYP-017..024 | Not started | readiness checklist | model-only tests under `tests/` | labeled `bounded_model` | no concurrent real execution |
| HYP-034..037 | COMPLETE | `hypervisor/virtual_timer.hh`, vCPU context, per-VM counter offset | ARM64 EL2 saves/restores CNTV state and programs/restores `CNTVOFF_EL2` | real guest `[HV-GT] virtual-timer-state` and `[HV-H] virtual-timer-irq` PASS with nonzero offset | None for basic state persistence, offset isolation, and event injection |
| HYP-038..040 | IN PROGRESS | bounded production timer lifecycle state | expiry-on-reentry, single-pending delivery, acknowledgement, re-arm, and cancellation | `virtual_timer_lifecycle` plus existing modeled migration/timer lanes PASS | deadline-driven descheduled-vCPU wakeup, real cross-CPU migration, and concurrent cancellation stress remain |

## Batch 0083 ownership evidence

| Requirement | Status | Design | Implementation | Evidence | Limitation |
|---|---|---|---|---|---|
| PRD-003 | In progress | `docs/architecture/alignment/ARCHITECTURE_ALIGNMENT_BATCH_0083.md` | test hooks and fixtures under `tests/`; production no-op hook interface | `tools/release/check_user_kernel_boundary.sh`, ARM64/AMD64 release builds | some bootstrap policy remains in kernel thread creation until the process server exists |
| PRD-017 | Complete | same | hypervisor guest fixture and control models under `tests/` | certification build plus boundary gate | none for source ownership |
| DOC-001..006 | In progress | `docs/README.md` | canonical `docs/` hierarchy | `tools/doc/check_layout.sh` | broader design content still requires ongoing reconciliation |
| USR-001..004 | In progress | `src/user/bootstrap/embedded_images.md` | user-owned bootstrap payload bundle and independent service ELFs | ARM64 certification build/runtime baseline | embedded registry is temporary and not a userspace process manager |

## Batch 0090 evidence

| Requirement | Status | Implementation | Runtime/negative evidence | Limitation |
|---|---|---|---|---|
| CAP-007 | IN PROGRESS | `capability/cspace.hh` rights attenuation | `capability_control` rejects escalation | Transfer and scalable CSpace work remain |
| CAP-008 | IN PROGRESS | bounded derivation parent records | 128 derive/revoke/reuse cycles | Bounded table, no scalable CDT |
| CAP-009 | IN PROGRESS | `capability::copy` with parent tracking | `capability_control` | Concurrency stress incomplete |
| CAP-006/010 | COMPLETE | `capability/cspace.hh`, `syscall/ipc.hh`, `thread/thread.hh`, `docs/readiness/IPC_BADGE_PROTOCOL.md` | `ipc_badge_delivery`, `ipc_badge_authority_snapshot`, userspace memory-server ownership protocol | Badge zero remains valid for deliberately unbadged bootstrap authority |
| CAP-011 | IN PROGRESS | locked atomic `capability::move` | compile/integration baseline | Dedicated runtime race evidence missing |
| CAP-012 | IN PROGRESS | single-slot delete | descendant removal negative checks | Concurrent lookup evidence missing |
| CAP-013 | IN PROGRESS | global descendant revoke | child/grandchild become absent | Scalable/restartable revoke missing |
| CAP-016 | IN PROGRESS | generations plus reusable derivations | repeated reuse loop | Long soak missing |
| CAP-017 | IN PROGRESS | one-cap IPC mint transfer on calls and replies | pager plus three-client memory-server integration | Multi-capability transfer and long-duration race evidence missing |
| CAP-019 | IN PROGRESS | direct-call rollback and reply-transfer retry preserve authority | occupied destination rollback in memory-server protocol | Multi-capability fault injection matrix missing |
| IPC-003 | IN PROGRESS | generation-checked single-use reply authority | pager two-client lifecycle | Not first-class reply object |
| IPC-004 | IN PROGRESS | reply-receive path | pager service integration | Atomicity stress incomplete |
| IPC-005 | IN PROGRESS | endpoint/thread cancellation paths | existing lifecycle suite | Race fuzz incomplete |
| IPC-009 | IN PROGRESS | one-cap transfer descriptor on call and reply | receiver-selected memory-server frame delivery | Multi-capability transfer absent |

| CAP-008..013 | IN PROGRESS | bounded CDT, copy/mint/move/delete/revoke in `capability/cspace.hh` | 128 derive/revoke/reuse cycles and attenuation negatives | fixed storage; concurrency and restartable revoke open |
| CAP-016 | IN PROGRESS | generation checks plus reusable derivation records | destroy/reuse and capability-control runtime suites | long soak and concurrent ABA proof open |
| CAP-017, CAP-018, CAP-019 | IN PROGRESS | single-cap call/reply transfer, receiver-selected destination, and retry-safe reply authority in `syscall/ipc.hh` | three-client memory-server delivery plus occupied-slot rollback | general receive windows, multi-cap transfer, and fault-injection matrix open |
| IPC-007, IPC-013, IPC-014 | IN PROGRESS | bounded timeout/donation cleanup and receiver-CPU wakeup paths | certification integration baseline | timeout ABI, race fuzz, and fully targeted teardown IPIs open |
| IPC-005..006, IPC-014 | IN PROGRESS | generation-checked endpoint queues and targeted ARM64 wake SGIs | final endpoint database validation rejects dead/duplicate/conflicting membership; `cross_cpu_wake_latency` is completed by the selected receiver CPU | `ipc_lifecycle_races`, object reuse, four-CPU workloads, endpoint invariants, and targeted wake samples PASS | Forced instruction-level cancellation interleavings, targeted teardown signaling, and non-ARM target IPIs remain |
| IPC-003..016 / IPC-GATE | COMPLETE | bounded endpoint lifecycle, four-capability authority transaction, frame-grant OOL transport, nonblocking notifications, targeted wakeups, and timing/footprint gates | `ipc_capability_batch` proves successful call and reply batches plus partial rollback; `ipc_ool_frame_grant` maps/writes/reads/unmaps/releases grants in three clients; `ipc_completion_gate`, lifecycle races, SMP fuzz, final invariants, latency bound, and release instruction budgets PASS | OOL payloads intentionally use memory capabilities rather than kernel buffering; AMD64 SMP wake delivery remains outside the current inactive platform |

## Batch 0092 evidence

| Requirement | Status | Implementation | Verification | Remaining limitation |
|---|---|---|---|---|
| IPC-006 | IN PROGRESS | `thread::wake()` performs an atomic transition only from blocked states; suspended/terminated threads cannot be resurrected by late reply or cancel paths. | ARM64 certification and release compile gates; four-CPU runtime rerun required. | Exhaustive reply/cancel/destroy race stress and first-class reply objects remain open. |
| SEC-017 | IN PROGRESS | Remote execution quiescence plus state-conditional wake protects user-bundle reclamation. | Existing SMP lifecycle suite plus pending 0092 runtime confirmation. | Other object classes still need explicit quiescence protocols. |

## Batch 0093 additions

| Requirement | Status | Implementation | Evidence | Remaining limitation |
|---|---|---|---|---|
| IPC-006 | IN PROGRESS | `thread::compare_state`, conditional wake/dispatch transitions, remote quiescence | ARM64 certification compile; runtime rerun pending | Exhaustive cancellation/reply/destroy interleavings are not fuzzed |
| SEC-017 | IN PROGRESS | Atomic scheduler execution claim prevents suspended-thread resurrection | ARM64/AMD64 release builds and boundary gates | Other object classes still need quiescence protocols |
| TST-018 | IN PROGRESS | Existing pager and object reuse suites exercise delayed teardown races | Four-CPU runtime confirmation pending | Dedicated controlled-scheduling race fuzz is absent |


### Batch 0094 evidence

| Requirement | Implementation | Evidence | Status | Remaining limitation |
|---|---|---|---|---|
| IPC-006 | `thread/scheduler.hh`: return-frame ownership remains busy until switch/idle commit | ARM64 four-CPU object destroy/reuse certification | IN PROGRESS | Controlled scheduler-vs-destroy race fuzz still required |
| SEC-017 | User-thread execution and return-frame quiescence protocol | Root-created SMP and object reuse integration suite | IN PROGRESS | IRQ, VM/vCPU, and remaining object teardown protocols are open |

| SEC-016..017 | In progress | `src/kernel/include/sys/kernel/thread/scheduler.hh` | generation-tagged CPU/thread binding and quiescent teardown | `object_destroy_reuse`, SMP fuzz | runtime rerun required for batch 0095 | other object classes and controlled race fuzz remain open |

### 0096 transactional process publication evidence

| Requirement | Status | Implementation | Runtime evidence required | Limitation |
|---|---|---|---|---|
| IPC-006 / SEC-017 | IN PROGRESS | `initialize_user()` leaves slots inactive; `create_user_bundle()` publishes `ready` only after the complete object/capability/address-space transaction commits. | Four-CPU `object_destroy_reuse` and full root-only acceptance pass without delayed user faults. | General multi-object transactional construction and controlled race-fuzz evidence remain open. |


### 0097 kernel-root quiescence evidence

| Requirement | Status | Implementation | Runtime evidence required | Limitation |
|---|---|---|---|---|
| MEM-019 / SEC-017 | IN PROGRESS | EL1-idle commits call `arch::space::activate_kernel()` before clearing the previous thread execution claim or CPU binding. Blocking no longer publishes quiescence while the CPU still executes through the reclaimable user TTBR0 root. | Four-CPU root-created object and destroy/reuse suites must pass without EL1 instruction aborts in the kernel identity region. | Kernel mappings still reside in TTBR0; final architecture must move permanent kernel mappings to TTBR1 and add generalized address-space residency tracking. |

- CAP-013 runtime correction (0099): two-phase mark/remove revoke preserves ancestry while discovering all descendants; `capability_control` exercises child + grandchild removal for 128 reuse cycles.

## Batch 0100 — dynamic physical-memory lifecycle foundation

| Requirement | Implementation | Verification | Status / limitation |
|---|---|---|---|
| MEM-001–MEM-005, USR-002, USR-007 | `boot/fdt.hh`, ARM64 boot DTB preservation, `memory/manager.hh`, and bootinfo v2 import DTB RAM/reservations, build bounded discontiguous allocator regions, and export inventory metadata to PL3 root. | ARM64 certification bootstrap validates region ordering, bitmap offsets, counts, and bootinfo totals; runtime log reports discovered region count. | In progress: bounded arrays, no untyped/resource capabilities, no userspace memory-server ownership, and no malformed-DTB fuzz corpus. |
| MEM-007, MEM-008, MEM-010 | Serialized physical allocator, owner-tagged frame/page-table objects, quota charging, and balanced destroy accounting. | `memory_resource_lifecycle` creates eight frames and four page tables, verifies peak ownership, destroys all objects, and verifies accounting returns to baseline. | In progress: untyped delegation, scalable pools, and pressure policy remain open. |
| MEM-015, MEM-019 | Bounded reverse mapping records plus `unmap_all()` during process bundle teardown. | Existing pager/process teardown, object reuse, and four-CPU lifecycle certification. | In progress: scalable mapping index and revoke/map race testing remain open. |
| TST-023 | Bounded create/destroy/accounting lifecycle test. | `[TEST] name=memory_resource_lifecycle`. | In progress: complete exhaustion and sustained pressure are not yet covered. |


## Batch 0102 — generation-safe mapping database foundation

| Requirement | Implementation | Verification | Status / limitation |
|---|---|---|---|
| MEM-014 | One frame supports up to eight serialized mappings. | `memory_mapping_database` maps one frame at two VAs and retains both records. | In progress: bounded fixed-size storage. |
| MEM-015 | Reverse records store the address-space object ID, generation, type, VA, permissions, and mapping generation. | Partial unmap and second-map retention are verified from PL3. | In progress: no scalable global index. |
| MEM-016 | Mapping records retain the frame and address-space capability derivations that authorized them; delete/revoke removes matching mappings before capability invalidation. | `memory_mapping_database` and `memory_authority_revoke` verify multi-map busy semantics and descendant-authority revoke cleanup. | In progress: bounded linear database, no scalable index, and controlled concurrent revoke/map/TLB race evidence remains open. |
| TST-023 | Bounded mapping lifecycle and rollback cleanup are part of root certification. | `[TEST] name=memory_mapping_database result=PASS`. | In progress: exhaustion, fragmentation, and sustained pressure remain open. |

| CAP-014 | A global capability-authority transaction lock serializes capability copy/mint/move/delete/revoke against map/unmap authority capture. | `memory_authority_revoke` verifies a descendant frame capability cannot retain a mapping after ancestor revoke. | In progress: IPC transfer/reply paths are not yet fully covered by the same controlled race suite. |
| CAP-015 | Mapping records bind resource use to capability derivations; frame destruction remains busy until all mappings are removed, including revoke-driven cleanup. | `memory_authority_revoke` revokes a derived frame capability and then destroys the ancestor-owned frame successfully. | In progress: generalized object reference quiescence and restartable revoke remain open. |
| TST-019 | Deterministic revoke-driven unmap integration coverage is present. | `memory_authority_revoke` is reported by the PL3 certification ledger. | In progress: concurrent revoke/map/unmap and TLB-shootdown race fuzz is not yet implemented. |

| MEM-017 | Public mapping attributes and architecture descriptor selection validate normal-inner and device mappings. | `[TEST] name=memory_attributes_pressure result=PASS`. | In progress: additional cache policies and cross-platform validation remain open. |
| MEM-018 | Root-only device-frame creation is restricted by the platform MMIO allowlist; device mappings are non-executable. | `[TEST] name=memory_attributes_pressure result=PASS`. | In progress: capability-based device database and driver delegation remain open. |
| TST-024 | Invalid attribute combinations and failed maps leave frame accounting balanced. | `[TEST] name=memory_attributes_pressure result=PASS`. | In progress: systematic allocation-stage fault injection remains open. |


## Batch 0109 — userspace memory-resource delegation

| Requirement | Implementation | Evidence | Status / limitations |
|---|---|---|---|
| MEM-005, MEM-006, MEM-009 | `memory::resource`, root selector 32, per-task selector 15, parent/child quota delegation | `memory_resource_delegation` certification test | Bounded resource objects; no physical extent retyping |
| MEM-007, MEM-008, MEM-010 | resource-backed frame/page-table creation and atomic used-page accounting | quota-two allocation/exhaustion/release cycle | Fixed object pools and simple quota policy |
| USR-006, USR-008, USR-011, USR-012 | PL3 memory server uses `resource_frame_create` through delegated selector 15 | userspace pager service plus resource exhaustion test | General allocation protocol and pressure policy remain open |

## Batch 0111 — physical extent ownership and bounded retyping

| Requirement | Implementation evidence | Runtime/certification evidence | Remaining limitations |
|---|---|---|---|
| MEM-006, MEM-009 | `memory::resource_extent`; parent delegation carves page-aligned physical ranges and child destruction merges empty ranges back. | `[TEST] name=memory_extent_retype result=PASS`. | Bounded 16 extents per resource; no scalable tree or restartable retype operation. |
| MEM-007, MEM-008 | Resource-backed frame and page-table creation allocates only from the authority resource's owned extents. | Existing pager/resource tests plus nested parent/child allocation exhaustion. | Fixed frame/page-table metadata pools remain. |
| MEM-010, MEM-012 | Quota, extent ownership, global allocation bitmap, zero/release, and parent/child accounting are enforced together. | Parent and child each exhaust exactly their physically owned pages, then return them without leaks. | Controlled concurrent split/retype/reclaim fuzz remains open. |
| TST-023 | Nested four-page parent/two-page child split, retype, exhaustion, destroy, merge, and reuse sequence. | `[TEST] name=memory_extent_retype result=PASS`. | Full-RAM exhaustion, fragmentation permutations, and long-duration pressure remain open. |

## Batch 0112 — scalable extent metadata and fragmentation recovery

| Requirement | Implementation evidence | Runtime/certification evidence | Remaining limitations |
|---|---|---|---|
| MEM-006, MEM-009 | Shared 256-node extent pool; per-resource sorted linked lists; whole-node transfer, split-node creation, rollback, and adjacent coalescing. | `[TEST] name=memory_extent_metadata result=PASS`. | Bounded global pool; no unbounded tree or restartable operation. |
| MEM-010, MEM-012 | Deterministic physical ordering, reusable metadata nodes, and global allocation bitmap preserve non-overlap and balanced ownership through fragmented return. | Twenty one-page children are returned in alternating order and successfully redelegated as one twenty-page resource. | Controlled concurrent split/retype/reclaim fuzz remains open. |
| TST-023 | Fragmentation and metadata-reuse certification sequence. | `memory_extent_metadata` plus existing `memory_extent_retype`. | Full-RAM near-exhaustion and long-duration pressure remain open. |

| TST-023 | IN PROGRESS | `src/user/init/main.cc`, `src/kernel/include/sys/kernel/memory/manager.hh` | `memory_pressure_rollback` drives 32 quota-exhaustion/reclaim cycles and 512 frame lifecycles with invariant signatures | Multi-CPU allocator pressure and full-RAM exhaustion remain open. |
| TST-024 | IN PROGRESS | `tests/include/sys/kernel/verification/hooks.hh`, `src/kernel/include/sys/kernel/memory/manager.hh` | Certification injects an extent-node split failure and verifies complete rollback through identical before/after resource signatures | Remaining allocation and teardown stages are not yet injectable. |

| USR-008 | IN PROGRESS | `include/abi/sys/v1/memory.hh`, `src/user/servers/memory/main.cc` | `memory_server_protocol` | Bounded handle protocol; no capability return yet |
| USR-011 / TST-023 | IN PROGRESS | `src/user/tests/memory_client/main.cc` | Three PL3 clients, 768 allocation/release calls | Full-RAM and long-duration pressure open |

| CAP-018 | IN PROGRESS | receiver-selected destination in memory-server reply protocol | occupied-slot negative test and successful frame-cap delivery | general receive windows remain open |

| IPC-006 / USR-017 | IN PROGRESS | `src/kernel/include/sys/kernel/syscall/control.hh`, `src/user/include/sys/thread.hh` | `memory_server_protocol` | Atomic exit notification removes the completion/teardown gap; general process wait/status and controlled race fuzz remain open |

| IPC-003, IPC-005..007 / SEC-017 / TST-018 | IN PROGRESS | `thread/thread.hh`, `syscall/ipc.hh`, `thread/scheduler.hh`, `syscall/control.hh` | `ipc_lifecycle_races` plus subsequent pager/memory-server endpoint reuse | Global correctness-first lifecycle lock; scalable locking, instruction-level race fuzz, and long-duration SMP evidence remain open |
| CAP-014, CAP-021 / SEC-018 | IN PROGRESS | `capability/cspace.hh`, `syscall/control.hh`, `syscall/ipc.hh` | `capability_transfer_revoke_race` verifies the receiver has no descendant after a cross-CPU transfer/revoke race; public reference/descendant revoke is locked by construction | Scalable authority locking, remaining mutation interleavings, and long-duration race fuzz remain open |
| CAP-012, CAP-016 | IN PROGRESS | `capability/cspace.hh` | Locked lookup snapshots run through the four-CPU certification fuzz and destroy/reuse suites | Controlled instruction-level lookup/delete interleaving and object-use quiescence remain open |
| CAP-014 | IN PROGRESS | `capability/cspace.hh`, `syscall/control.hh`, `syscall/ipc.hh` | Public mutation APIs are authority-locked by construction; existing capability lifecycle, transfer/revoke race, SMP fuzz, and destroy/reuse tests pass | Global lock is not scalable; controlled coverage of every mutation pairing remains open |
| CAP-012, CAP-015..016 / SEC-016, SEC-018 | IN PROGRESS | `object/table.hh`, `arch/arm64/arch.cc` | `object_lookup_destroy_race` repeatedly signals on CPU 1 while CPU 0 revokes, unregisters, and destroys the notification | Fixed global grace-period scan; non-exception readers and long-duration generation wraparound remain open |
| SEC-004 | COMPLETE | `arch/arm64/kernel.ld`, `arch/arm64/boot/start.S`, `arch/arm64/include/sys/arch/memory.hh`, `arch/arm64/include/sys/arch/stack.hh`, `arch/arm64/arch.cc` | Bootstrap validates eight unmapped guard PTEs and their adjacent usable pages; clean four-CPU certification validates bounds/canaries on every exception | Scalable/dynamic CPU stack allocation is tracked separately from the guard-page requirement |
| SEC-006 | COMPLETE | `memory/manager.hh`, `thread/scheduler.hh`, `hypervisor/lifecycle.hh` | Bootstrap poisons, releases, reallocates, and verifies a physical page; hypervisor certification poisons and verifies complete vCPU architectural/timer/interrupt/exit-state teardown; full object reuse acceptance passes | Device MMIO is intentionally never scrubbed by CPU stores |
| HYP-043 | COMPLETE | `arch/arm64/include/sys/arch/hypervisor.hh` | Unknown values are rejected by the ABI classifier and become bounded hypercall exits with the original number in `qualification`; negative classifier certification passes | The stable public hypercall ABI remains HYP-042 |
| SEC-007 / HYP-046 | IN PROGRESS | `arch/arm64/include/sys/arch/hypervisor.hh` | Hostile all-ones and zero SCTLR requests verify allowed-bit masking and mandatory RES1 restoration; real guest MMU bring-up passes | Other trapped/programmed system registers still require an explicit mask audit |
| SEC-011 | COMPLETE | `arch/arm64/include/sys/arch/memory.hh`, `thread/scheduler.hh` | Feature-detected PAN enable and UAO disable are read back during bootstrap certification; clean four-CPU acceptance passes | Explicit user-copy windows remain tracked by SEC-005 |
| HYP-047 | COMPLETE | `arch/arm64/include/sys/arch/hypervisor.hh`, `hypervisor/object.hh` | Guest abort records retain ESR, FAR, guest PC, and reconstructed IPA; classifier/IPA negative checks and real guest execution pass | Userspace VMM delivery policy remains separate from record completeness |
| SEC-023 | COMPLETE | `thread/scheduler.hh`, `fault/fault.hh` | `fault_ipc_delivery` and two real pager faults recover before the complete four-CPU acceptance suite reaches zero failures | Kernel-internal corruption remains covered by SEC-019..021 |
| SEC-024 | COMPLETE | `arch/arm64/include/sys/arch/hypervisor.hh`, `hypervisor/vcpu.hh` | Stage-2 faults are recoverable exits; unexpected traps fault only the owning vCPU/VM; multi-VM isolation and negative hypervisor fuzz pass | Device-originated failures remain DEV-018 |
| SEC-001..003 | COMPLETE | `arch/arm64/kernel.ld`, `arch/arm64/include/sys/arch/memory.hh`, `arch/arm64/include/sys/arch/space/address_space.hh`, `hypervisor/stage2.hh` | Bootstrap walks both kernel image L3 tables and verifies text RX, embedded images/rodata RO-NX, data/BSS RW-NX, and no writable executable PTE; user/stage-2 W^X negative tests and full acceptance pass | Later loadable kernel modules must preserve the same mapping policy |
| SEC-020..021 / OBS-002, OBS-004 | COMPLETE | `kernel/emergency.hh`, `kernel/printk.hh`, `arch/arm64/arch.cc`, `arch/arm64/kernel.ld` | Bootstrap publishes and reads back a CPU-local record; ELF inspection places the checksummed crash record in `.noinit`; four-CPU acceptance passes with bounded console contention | Formatted asynchronous ring draining remains OBS-003 |
| OBS-005..006, OBS-009..010 | COMPLETE | `kernel/emergency.hh`, `arch/arm64/arch.cc`, `syscall/ipc.hh`, `thread/scheduler.hh`, `hypervisor/vcpu.hh`, `mk/config.mk` | Certification traces all required subsystems; release builds with `CONFIG_TRACE=0`; release ELF string audit excludes guest registers and user/guest address diagnostics; format v1 is documented | External trace export/draining remains OBS-003 |
| SCH-018 | COMPLETE | `kernel/printk.hh`, `kernel/emergency.hh`, `docs/readiness/DIAGNOSTIC_RECORD_FORMAT.md` | `printk::defer` publishes bounded structured records without console lock or IRQ masking | Formatted asynchronous draining remains OBS-003 |
| PRD-003, PRD-013..017 / SEC-012 | COMPLETE | `kernel/hypervisor/{object,stage2,virtual_irq,virtual_timer,lifecycle,vmid}.hh`, `arch/arm64/include/sys/arch/hypervisor.hh`, `tests/include/sys/kernel/tests/hypervisor`, `syscall/ipc.hh` | Certification acceptance passes; release ELF audit contains no fuzz/profile/acceptance/HV-walk strings; release denies guest diagnostics and has no fuzz discriminator path | Intentional guest console ABI remains governed by HYP-041..042 |
| HYP-004 / OBS-007 | COMPLETE | `object/table.hh`, `hypervisor/{object,stage2,vcpu,lifecycle}.hh` | Certification creates and destroys a dynamic object, maps/unmaps guest pages, executes the guest, and verifies live counts and entry/exit balance; full acceptance and release builds pass | Counters are bounded in-memory diagnostics and are not yet exported to a userspace metrics service |
| OBS-008 | IN PROGRESS | `hypervisor/{object,stage2,vcpu,lifecycle}.hh` | Sequence-published audit records cover reset, map/unmap, run entry/exit, pause/resume, stop, and teardown in certification | Device assignment is not implemented, so assignment audit records remain open |
| SEC-015 / TST-029 | IN PROGRESS | `object/table.hh`, `hypervisor/{object,stage2,vcpu}.hh` | Checked object and VM counters detect saturation, underflow, and lifecycle imbalance; bounded create/destroy and map/unmap/run balance checks pass | Complete checked-counter audit and long-duration no-growth soak remain open |
| SEC-013..014 / SCH-015 | COMPLETE | `kernel/lock/order.hh`, `capability/cspace.hh`, `ipc/endpoint.hh`, `thread/thread.hh`, `memory/manager.hh`, `object/table.hh` | Certification verifies legal nesting, recursion rejection, equal-rank ordering, strict LIFO release, and zero violations through the complete four-CPU acceptance workload | The bounded printk lock remains intentionally independent and uses emergency-ring fallback |
| DOC-011 | IN PROGRESS | `docs/readiness/LOCKING_PROTOCOL.md` plus subsystem lifecycle documentation | Lock hierarchy and checker behavior are independently documented and certification-backed | IRQ and future device teardown protocols remain open |
| SEC-019 | COMPLETE | `kernel/panic.hh`, `arch/arm64/arch.cc`, `kernel/emergency.hh` | `scheduler_independent_panic` poisons current-thread identity, holds the printk lock, captures a synthetic fatal context, and validates its checksummed `.noinit` record; full acceptance passes | Platform watchdog/reset integration remains SEC-022 |
| PRD-018 | COMPLETE | `tools/release/check_source_boundaries.sh`, `mk/checks.mk` | `boundary-check` passes and is part of `production-gate` | Limits can only be raised through reviewed build-policy changes |
| TST-036 | COMPLETE | `mk/checks.mk`, `tools/release/check_reproducible_build.sh`, deterministic earlyfs tar creation | `make BUILD_VARIANT=release reproducible-check` passes for ARM64 and AMD64 with `SOURCE_DATE_EPOCH=0` | Linker map paths are normalized for comparison; future toolchain changes must preserve artifact determinism |
| TST-031/032 | IN PROGRESS | `tools/release/check_static_analysis_tools.sh`, `mk/checks.mk` | `static-analysis-tools-check` inventories required tools and writes a deviation report | `scan-build` and `clang-tidy` are absent; install them and run project-specific profiles before release sign-off |
| TST-033 | COMPLETE | `tools/abi/check_ubsan.sh`, `tests/abi/layout.cc`, `mk/checks.mk` | `make ubsan-check` passes with UBSan recovery disabled | Scope is portable ABI/layout code; freestanding architecture-specific kernel code requires a separate sanitizer-capable harness |
| TST-034 | COMPLETE | `mk/toolchain.mk`, `tools/release/check_stack_usage.sh`, `mk/checks.mk` | ARM64 and AMD64 release gates emit and validate `.su` records with an 8 KiB per-function limit | Bound is per function; cumulative interrupt-path call depth remains covered by retained runtime stack guards |
| SEC-005 | COMPLETE | `kernel/user_access.hh`, `arch/arm64/space/address_space.hh` | `user_range_and_arm_hardening` covers readable, read-only, writable, unmapped, kernel-crossing, and arithmetic-wrap ranges | No production syscall currently accepts bulk pointer arguments |
| IPC-018 | IN PROGRESS | `thread/scheduler.hh`, `fault/fault.hh` | Undefined EC classification is certification checked and feeds the fault-IPC record kind/address path | Real PL3 UDF delivery/reply test remains open |
| SEC-008 | IN PROGRESS | `arch/arm64/hardening.hh`, `kernel/user_access.hh` | All four CPUs publish feature inventory and execute CSDB+ISB at initialization; range validation uses the same barrier | Real supported-platform CSV or firmware mitigation qualification remains open |
| SEC-009..010 | COMPLETE | `docs/readiness/KERNEL_SECURITY_MODEL.md`, `arch/arm64/hardening.hh` | CPU feature inventory is certification checked; release/certification builds pass | PAuth and BTI are deliberately inactive until complete assembly coverage is delivered |
| SCH-006 / TIM-005 / PLT-013 / DOC-006 | COMPLETE | `docs/readiness/UNSUPPORTED_1_0.md` | Production scope rejects implicit claims for hotplug, suspend, and AMD64 virtualization | Implementing any item requires a future runtime-certified release |
| DOC-007..008, DOC-010 | COMPLETE | `docs/readiness/KERNEL_SECURITY_MODEL.md`, `docs/readiness/MEMORY_ORDERING.md` | Documentation matches capability, stage-2, atomic publication, TLBI, MMIO, and panic implementations | Device/SMMU trust boundary will extend when that subsystem exists |
| MEM-021 | COMPLETE | `arch/arm64/space/asid.hh`, `arch/arm64/space/address_space.hh`, `thread/{thread,scheduler}.hh` | `asid_rollover_reuse` advances the generation with live address spaces, refreshes root, then real PL3 scheduling and destroy/reuse pass | Allocator is intentionally bounded to 63 simultaneous current-generation identifiers |
| HYP-013 / TST-021 | COMPLETE | `hypervisor/{vmid,object,stage2,vcpu,lifecycle}.hh`, `tests/hypervisor/control_models.hh` | `vmid_rollover_reuse` advances the generation with a live VM; real guest execution and all lifecycle/teardown/reuse models pass afterward | VM object creation remains bounded by the separate lifecycle implementation |
| IPC-011 / SCH-011..013 | COMPLETE | `scheduling/context.hh`, `scheduling/context.md`, `syscall/ipc.hh` | `scheduling_context_donation`, `priority_inheritance`, and `donation_chain_bound`; cancellation/timeout/exit/teardown suites remain green | Measured RT latency and long-duration inversion stress remain separate SCH-020..024 gates |
| SCH-014 | COMPLETE | `thread/scheduler.hh`, `docs/readiness/LOCKING_PROTOCOL.md` | `timeout_queue_order` plus IPC and pager timeout integration; full lock-order certification reports zero violations | Queue capacity follows the bounded 1.0 thread capacity |
| SCH-008/009 | COMPLETE | `scheduling/context.hh`, `syscall/control.hh`, `scheduling/context.md` | `scheduling_configuration` rejects live mutation, invalid affinity, priority truncation, zero budget, and budget-over-period; valid suspended migration passes | Six logical hours and 21,600 periods pass exact accounting and deadline checks |
| IRQ-002/003/005 | IN PROGRESS | `docs/readiness/INTERRUPT_LIFECYCLE.md` | exclusive registry, capability-authorized bind/ack, guarded cross-CSpace delegation | `irq_ownership_delegation`; real timer path remains green | external IRQ discovery/publication and real device trigger tests remain |
| IRQ-004/006..008 | COMPLETE | `kernel/interrupt.hh`, ARM64 GIC backend, exception dispatch | `irq_ack_deactivate`, `irq_storm_containment`, capability revoke, full timer/IPI/SMP acceptance | Shared physical lines are explicitly unsupported; userspace may demultiplex one owned line |
