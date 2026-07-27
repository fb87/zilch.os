# Product/Test Separation Batch 0079

## Scope

This batch establishes structural boundaries between the native production ABI and the certification harness.

## Implemented

- Production `sys::abi::v1::control_operation` contains only product operations.
- Production `sys::abi::v1::hypervisor_operation` contains only product operations.
- Certification selectors live in `tests/include/sys/test_abi/v1/certification.hh` under the `sys::test_abi` namespace.
- Certification userspace receives test include paths only when `CONFIG_SELFTEST=1`.
- Kernel test dispatch is compiled only when `CONFIG_SELFTEST=1`.
- The ARM64 guest verification payload is compiled only when `CONFIG_HYPERVISOR_SELFTEST=1`.
- Model-only hypervisor results use `HV-MODEL` and `hypervisor_control_model` labels.
- The release ELF gate rejects certification strings, model labels, root-fuzz markers, and embedded guest payload symbols.

## Truthful status

This batch completes the public-ABI cleanup and guest-fixture exclusion portions of Alignment Milestone A. It does not yet move all hypervisor model implementation out of `hypervisor.hh`; that remains an open module-boundary task.

## Verification

- ARM64 certification build: PASS.
- ARM64 release build and production ELF gate: PASS.
- AMD64 release compatibility build and production ELF gate: PASS.
- Production source gate: PASS.

Runtime certification remains required on an ARM64 QEMU host to confirm that the separated test ABI produces the same acceptance result as 0078.
