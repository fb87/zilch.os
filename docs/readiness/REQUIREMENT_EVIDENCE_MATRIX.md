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
