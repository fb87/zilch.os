# Requirement-to-evidence matrix

Status follows `docs/readiness/PRODUCTION_READINESS_CHECKLIST.md`. A passing bounded test is evidence of a foundation, not proof of a production gate.

| Requirement | Status | Design | Implementation | Verification/evidence | Limitation |
|---|---|---|---|---|---|
| PRD-001/002 | Complete | Product/test separation | `Makefile` build variants | ARM64 certification and release builds | None for configuration boundary |
| PRD-003 | Complete | Product/test separation | `tests/include/`, conditional guest target | release ELF/source gates | Test code excluded from release; test headers remain in repository |
| PRD-004 | Complete | Boot flow | release build | product boot log with selftests disabled | QEMU evidence retained externally |
| PRD-005 | Complete | Guest ownership | `src/user/guests/test-arm64/` | guest-boundary and production ELF gates | Certification uses temporary blob adapter |
| PRD-007..009 | Complete | ABI separation | `include/abi`, `tests/include/sys/test_abi` | production ELF gate | None |
| PRD-010..012 | In progress | `docs/architecture/abi/ABI_COMPATIBILITY.md` | `include/abi/sys/v1`, `tests/abi/layout.cc` | `make abi-check` | ABI remains C++-only and not frozen |
| PRD-013..017 | Complete | Hypervisor module boundaries | `src/kernel/include/sys/kernel/hypervisor/`, `tests/.../control_models.hh` | ARM64 certification/release; source gate | Header-only implementation remains |
| PRD-018 | In progress | Module rules | module files below current 500-line limit | source line-count inspection | Automated dependency rule still open |
| IPC-017..020 | In progress | Fault IPC/pager docs | scheduler, IPC, memory server | two-client pager runtime PASS | pager death/timeout/nested faults open |
| USR-006/009 | In progress | userspace pager batch | `src/user/servers/memory` | `userspace_pager_service` PASS | bounded resources and certification policy |
| USR-013..015 | In progress | ELF bootstrap loader | ARM64 bootstrap ELF loader | pager/client and SMP reuse runtime PASS | path-based userspace loader, TLS/auxv open |
| HYP-016 | In progress | real guest execution | arch EL2 backend and kernel vCPU run | `hypervisor_real_single_vcpu` PASS | production userspace VMM absent |
| HYP-017..024 | Not started | readiness checklist | model-only tests under `tests/` | labeled `bounded_model` | no concurrent real execution |

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
| CAP-010 | IN PROGRESS | `capability::mint` with reduced rights and badge | escalation rejection | Badge delivery semantics incomplete |
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
