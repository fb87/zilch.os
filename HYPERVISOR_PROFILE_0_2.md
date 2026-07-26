# Zilch Hypervisor Profile 0.2

Profile 0.2 extends the certified single-vCPU Profile 0.1 guest with guest-owned
stage-1 translation, exception vectors, virtual interrupt delivery, and EL0
execution.

## Runtime sequence

1. EL2 installs the VMID-tagged stage-2 translation root.
2. Guest EL1 builds an identity-mapped 4 KiB stage-1 hierarchy for its 64 KiB
   IPA window and enables `SCTLR_EL1.M/C/I`.
3. Guest installs a 2 KiB-aligned `VBAR_EL1` and reports the MMU/vector gate.
4. Guest executes `WFI`; EL2 returns a structured `wait` exit.
5. The host marks virtual IRQ 27 pending and re-enters the vCPU with
   `HCR_EL2.VI` asserted.
6. Guest EL1 handles the IRQ, acknowledges it through the versioned HVC ABI,
   and reports the timer gate.
7. Guest enters EL0, handles `SVC #0` through the lower-EL vector, reports the
   EL0 gate, and uses `SVC #1` to request clean shutdown.

The shutdown exit carries a report mask. Acceptance requires all three bits:

- bit 0: guest MMU and vectors
- bit 1: virtual timer IRQ
- bit 2: guest EL0 SVC

## Diagnostics

Expected success records are:

```text
[HV-G] guest-mmu-vectors result=PASS
[HV-H] virtual-timer-irq result=PASS irq=27 reports=7
[HV-I] guest-el0-svc result=PASS
[HV-0.2] guest-mmu-timer-el0 result=PASS
[HV-DIAG] profile=0.2 self-test=PASS
```

Unexpected guest exits retain the Profile 0.1 syndrome, FAR, HPFAR, PC, PSTATE,
register, VMID, and run-generation diagnostics.

## Structured MMU diagnostics

Profile 0.2 emits `[HV-MMU]` records immediately before and after enabling
`SCTLR_EL1.M`. The record includes the guest EL1 translation registers, PAR,
stage-2 controls, bootstrap descriptors, target PC and stack. Early guest EL1
fault handlers report ESR, FAR, ELR and SPSR through the same MMU-independent
HVC path. The post-enable transition is an assembly-only `adr`/`br` trampoline.
