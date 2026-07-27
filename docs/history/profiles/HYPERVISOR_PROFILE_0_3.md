# Hypervisor Profile 0.3

Profile 0.3 extends the runtime-certified Profile 0.2 guest execution path with
bounded lifecycle, interrupt-controller, isolation, migration-accounting, and
VMID-reuse mechanisms. The real ARM64 guest remains the architectural execution
acceptance test; deterministic kernel self-tests cover the new control-plane
state machines.

## Scope

- Up to four vCPUs per VM in the bounded control-plane model.
- Per-vCPU pending, active, and masked virtual IRQ bitmaps for IRQs 0–63.
- Explicit configured, runnable, running, blocked, paused, stopped, and faulted
  vCPU states.
- Host-CPU migration accounting on each guest run.
- Two-VM stage-2 ownership/isolation validation.
- Ordered VM teardown: stop vCPUs, clear IRQ and architectural state, clear
  mappings, invalidate stage 2, and release the VMID.
- Deterministic lowest-free VMID allocation and safe reuse after invalidation.

## Acceptance

A runtime-certified build must retain all Profile 0.2 PASS lines and add:

```
[HV-J] guest-smp-model cpus=4 result=PASS
[HV-K] virtual-irq-controller result=PASS irqs=64
[HV-L] vcpu-lifecycle-migration result=PASS migrations=1
[HV-M] multi-vm-isolation result=PASS vms=2
[HV-N] vm-teardown-vmid-reuse result=PASS
[HV-0.3] multi-vcpu-multivm-lifecycle result=PASS
```

This profile does not yet claim concurrent execution of four guest vCPUs. It
certifies the bounded kernel mechanisms required for that next execution step.
