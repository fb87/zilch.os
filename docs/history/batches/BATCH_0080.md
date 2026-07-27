# Batch 0080 — user-owned guest and certification boundary

- Builds the ARM64 verification guest independently from `src/user/guests/test-arm64`.
- Packages `/guests/test-arm64.elf` only in certification earlyfs.
- Moves real-guest harness fixtures and bounded hypervisor control models to `tests/include/sys/kernel/tests/hypervisor/control_models.hh`.
- Removes numbered `profile*` fixtures and labels from production kernel source.
- Renames the production root-object module from `profile` to `bootstrap`.
- Keeps the generated guest blob adapter as a certification-only bridge until the userspace domain manager loads guest ELF files through production APIs.
