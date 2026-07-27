# Hypervisor Profile 0.6

Profile 0.6 extends the certified Profile 0.5 baseline with a bounded physical-CPU-lane dispatcher for independently saved vCPU contexts.

## Scope

- Four atomic physical execution lanes.
- Generation-checked vCPU ownership and handoff.
- Four-vCPU VM and two-vCPU VM interleaved across four host lanes.
- Cross-vCPU IPI and independent timer delivery during dispatch.
- Migration and VM-switch accounting.
- Busy rejection while a vCPU remains active.
- Quiescent teardown with no persistent additional VM/vCPU fixtures.

## Acceptance

```
[HV-AA] physical-cpu-lanes cpus=4 result=PASS
[HV-AB] generation-checked-handoff dispatches=576 result=PASS
[HV-AC] cross-cpu-virtual-events ipis=3 timers=6 result=PASS
[HV-AD] concurrent-multivm-reentry switches=192 result=PASS
[HV-AE] physical-lane-migration migrations=... result=PASS
[HV-AF] quiescent-teardown busy=1 result=PASS
[HV-0.6] physical-lane-vcpu-dispatch result=PASS
[TEST] name=hypervisor_profile_0_6 result=PASS
```

## Certification boundary

This profile certifies the kernel dispatcher, ownership, saved-context handoff, cross-CPU virtual events, migration, concurrent multi-VM interleaving, and teardown invariants. The existing Profile 0.2 guest remains the real ARM64 EL2/EL1/EL0 instruction-execution test. A later profile should bind this dispatcher to simultaneous secondary-core `enter_guest` calls and guest-side SMP rendezvous.
