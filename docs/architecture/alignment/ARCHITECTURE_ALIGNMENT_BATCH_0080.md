# Architecture Alignment Batch 0080

## Scope

This batch moves the ARM64 hypervisor verification guest out of the architecture
boot tree and makes it an independently linked user-owned executable.

## Ownership boundaries

- `src/kernel/` owns VM/vCPU, stage-2, exit, and interrupt mechanisms.
- `src/user/guests/` owns guest executable source and guest runtime code.
- certification-only orchestration may temporarily expose the guest binary to
  the in-kernel real-single-vCPU harness through a generated read-only blob.
- release builds compile neither the guest ELF nor the blob adapter.

The blob adapter is transitional. It is not evidence of a production userspace
VMM. The final path is earlyfs lookup by a PL3 domain manager, ELF loading into
delegated VM frames, and capability-authorized VM/vCPU lifecycle operations.

## Artifacts

Certification builds produce:

- `out/.../user/guests/test-arm64/guest-test.elf`
- `out/.../image/earlyfs.tar:/guests/test-arm64.elf`

Release earlyfs omits the verification guest.

## Gates

- guest source must remain under `src/user/guests/`;
- guest code may not include private kernel headers or namespaces;
- the independent guest ELF must use 4 KiB maximum page alignment;
- production ELF and earlyfs must contain no verification guest payload;
- the existing real-single-vCPU runtime result must remain green.
