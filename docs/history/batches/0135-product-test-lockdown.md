# Batch 0135: product/test lockdown

## Targets

- Complete PRD-003 production exclusion of self-test mechanisms.
- Complete PRD-013 through PRD-017 hypervisor module/test separation.
- Complete SEC-012 production debug-interface lockdown.

## Implementation

IPC fuzz counters, health reporting, request decoding, and the `x6` test discriminator now compile only with `CONFIG_SELFTEST=1`. The ordinary production IPC path cannot enter certification behavior.

The guest diagnostic hypercall is available only when verbose diagnostics are configured. Release kernels return `denied` and compile out detailed EL2 MMU/register/page-table console walks.

The architecture-independent VM/vCPU model, stage-2 manager, virtual interrupt/timer state, lifecycle, and VMID allocator are already separated into dedicated production headers. Modeled execution and guest fixtures reside in certification-only include and fixture paths.

## Evidence

The clean four-CPU certification suite retains all fuzz and hypervisor evidence and passes with zero failures. The release image builds successfully, and its string audit finds none of the fuzz-failure, SMP-fuzz, acceptance, hypervisor-control-model, `HV-MMU`, `HV-S2`, or `HV-TRAP` diagnostics.
