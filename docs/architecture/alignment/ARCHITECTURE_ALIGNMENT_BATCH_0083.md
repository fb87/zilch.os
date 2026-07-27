# Architecture Alignment Batch 0083

## Scope

This batch consolidates project documentation and completes the structural
ownership split between production kernel mechanisms, userspace payloads, and
certification code.

## Documentation layout

The repository root now contains only project entry points. Current documents
live under `docs/architecture`, `docs/readiness`, and `docs/roadmap`; historical
batch and profile records live under `docs/history`; focused certification
notes live under `docs/certification`.

`tools/doc/check_layout.sh` prevents new top-level Markdown files outside the
approved root entry points.

## User/kernel ownership split

- Root bootstrap image packaging moved from `src/arch/arm64/boot/user.S` to
  `src/user/bootstrap/embedded_images.S`.
- The certification guest blob adapter moved from the architecture tree to
  `tests/hypervisor/fixtures/guest_blob.S`.
- Acceptance state and result formatting moved from `src/kernel/` to a
  certification override under `tests/include/sys/kernel/verification/`.
- Production kernel code calls a narrow no-op verification hook interface.
- Scheduler test workers moved from `src/kernel/` to `tests/include/`.
- The build links user-owned bootstrap data and test-owned fixtures as explicit
  data objects rather than architecture or kernel implementation objects.

The embedded bootstrap adapter remains temporary. It is not a production
pathname-based process loader and must be removed after the userspace process
server can load `/bin/init` from delegated earlyfs resources.

## Verification

- ARM64 certification build: PASS
- ARM64 release build: PASS
- AMD64 release compile-only build: PASS
- ABI layout check: PASS
- documentation layout gate: PASS
- user/kernel ownership boundary gate: PASS
- production ELF gates: PASS
