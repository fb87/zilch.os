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
| CAP-017 | IN PROGRESS | one-cap IPC mint transfer | pager/IPC integration | Dedicated cross-CSpace runtime test missing |
| CAP-019 | IN PROGRESS | direct-transfer failure restores receiver | build and code-path review | Fault injection matrix missing |
| IPC-003 | IN PROGRESS | generation-checked single-use reply authority | pager two-client lifecycle | Not first-class reply object |
| IPC-004 | IN PROGRESS | reply-receive path | pager service integration | Atomicity stress incomplete |
| IPC-005 | IN PROGRESS | endpoint/thread cancellation paths | existing lifecycle suite | Race fuzz incomplete |
| IPC-009 | IN PROGRESS | one-cap transfer descriptor | direct/queued transfer paths | Multi-cap and receive windows absent |

| CAP-008..013 | IN PROGRESS | bounded CDT, copy/mint/move/delete/revoke in `capability/cspace.hh` | 128 derive/revoke/reuse cycles and attenuation negatives | fixed storage; concurrency and restartable revoke open |
| CAP-016 | IN PROGRESS | generation checks plus reusable derivation records | destroy/reuse and capability-control runtime suites | long soak and concurrent ABA proof open |
| CAP-017, CAP-019 | IN PROGRESS | single-cap IPC transfer and direct-rendezvous rollback in `syscall/ipc.hh` | pager and IPC integration; destination-occupied negative path | multi-cap receive windows and fault-injection matrix open |
| IPC-007, IPC-013, IPC-014 | IN PROGRESS | bounded timeout/donation cleanup and receiver-CPU wakeup paths | certification integration baseline | timeout ABI, race fuzz, and fully targeted teardown IPIs open |
