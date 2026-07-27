# Hypervisor Profile 0.1 guest-execution candidate

This milestone enables the first real ARM64 guest transition while preserving
all earlier capability, VMID, stage-2 policy, and diagnostic checkpoints.

## Bounded virtual platform

- one VM and one vCPU;
- 64 KiB private guest RAM;
- 4 KiB stage-2 translation with L1/L2/L3 tables;
- guest IPA range `0x00000000-0x0000ffff`;
- guest EL1, stage-1 MMU initially disabled;
- hypercall console, physical-counter query, and shutdown;
- no virtual GIC, virtio, passthrough, or SMP guest yet.

## Entry/exit ownership

The EL2 backend preserves the complete host HVC exception frame per physical
CPU. It installs the guest register context and stage-2 configuration into the
live EL2 return path. Guest hypercalls may resume the guest directly. Shutdown,
stage-2 faults, and unexpected traps save guest state, restore HCR/VTTBR/VTCR,
and restore the exact host frame before returning to the userspace VMM call.

## Diagnostics

Unexpected exits report exit reason, ESR_EL2, FAR_EL2, HPFAR_EL2, guest PC,
PSTATE, VMID, run generation, x0-x7, and guest SP_EL1. The host remains the
source of truth for the exit record.

## Runtime acceptance

Expected new records:

```text
[HV-D] guest-entry vmid=1 root=... ram=... size=65536
[GUEST] boot
[GUEST] time
[HV-E] guest-console-time-shutdown result=PASS ...
[HV-F] single-vcpu-zilch-guest result=PASS
[TEST] name=hypervisor_profile_0_1 result=PASS
```

A runtime failure should include `hv guest-exit` and `hv guest-regs` records.

## Profile 0.2 MMU bootstrap return diagnostics

The EL2-mediated guest MMU enable path invalidates guest EL1 translation and
instruction-cache state after writing `SCTLR_EL1`, then executes `AT S12E1R`
for the post-enable PC.  The `[HV-MMU] phase=el2-resume` record reports the
combined stage-1 plus stage-2 `PAR_EL1` result and the final EL1/EL2 control
register readbacks immediately before the common EL2 exception epilogue.
