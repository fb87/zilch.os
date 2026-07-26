# Hypervisor Profile 0.5

Profile 0.5 extends the runtime-certified Profile 0.4 control plane with
independently saved secondary-vCPU architectural contexts and deterministic
scheduler-driven re-entry. The ARM64 Profile 0.2 guest remains the real EL1/EL0
execution test; Profile 0.5 verifies the kernel mechanisms required before a
secondary guest CPU can be dispatched through the hardware guest-entry path.

## Scope

- Four independently initialized guest-vCPU architectural contexts.
- Secondary-vCPU off-to-starting-to-online boot transitions.
- Per-vCPU entry PC, EL1 stack, boot cookie, GPRs, and resume PC.
- 128 interleaved execution slices per vCPU across four host CPUs.
- Context preservation across preemption and migration.
- Cross-vCPU virtual IPI delivery and acknowledgement.
- Independent per-vCPU virtual timer state and IRQ delivery.
- Teardown rejection while any vCPU remains active.

## Acceptance

```
[HV-U] secondary-vcpu-entry cpus=4 result=PASS
[HV-V] independent-vcpu-contexts reentries=512 result=PASS
[HV-W] cross-vcpu-ipi ipis=3 result=PASS
[HV-X] scheduler-driven-reentry migrations=... result=PASS
[HV-Y] per-vcpu-timer-state events=4 result=PASS
[HV-Z] active-vcpu-teardown busy=1 result=PASS
[HV-0.5] secondary-vcpu-context-reentry result=PASS
[TEST] name=hypervisor_profile_0_5 result=PASS
```

## Certification boundary

This profile certifies independent secondary-vCPU context creation, lifecycle,
preemption, migration, interrupt state, and deterministic re-entry in the
kernel acceptance path. The next hardware-execution increment must dispatch
those secondary contexts through the ARM64 EL2 guest-entry path on multiple
physical CPUs and add guest-side secondary CPU code and barriers.

## Fixture lifetime

The Profile 0.5 acceptance path reuses the Profile 0.4 VM and four-vCPU
fixtures after Profile 0.4 completes ordered teardown. It does not reserve a
second persistent set of full vCPU objects in kernel BSS. This preserves the
object allocator headroom required by the root-created worker-bundle,
SMP-fuzz, and destroy/reuse acceptance tests that run after the hypervisor
suite.
