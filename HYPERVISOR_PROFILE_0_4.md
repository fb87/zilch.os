# Hypervisor Profile 0.4

Profile 0.4 extends the runtime-certified Profile 0.3 control plane with a
bounded concurrent-execution model for two VMs and six vCPUs. The existing
ARM64 Profile 0.2 guest remains the architectural execution test, while the
0.4 acceptance workload verifies scheduling, migration, virtual IPIs,
per-vCPU timers, isolation, and teardown ordering under interleaved load.

## Scope

- Four online vCPUs in VM A and two online vCPUs in VM B.
- Deterministic round-robin execution over four host CPUs.
- Per-vCPU run quanta and cross-CPU migration accounting.
- Per-vCPU virtual timer deadlines and IRQ 27 delivery.
- Virtual IPI injection, acknowledgement, and deactivation.
- Overlapping IPA ranges with isolated host backing memory.
- Teardown rejection while a vCPU is running.
- Ordered teardown followed by deterministic VMID reuse.

## Acceptance

```
[HV-O] guest-smp-online cpus=4 barrier=1 result=PASS
[HV-P] virtual-ipi-delivery ipis=3 result=PASS
[HV-Q] per-vcpu-timers irqs=6 result=PASS
[HV-R] vcpu-preemption-migration quanta=384 migrations=... result=PASS
[HV-S] concurrent-multivm vms=2 vcpus=6 result=PASS
[HV-T] teardown-under-load busy=1 vmid-reuse=PASS result=PASS
[HV-0.4] concurrent-vcpu-multivm-execution result=PASS
[TEST] name=hypervisor_profile_0_4 result=PASS
```

## Certification boundary

The workload is deterministic and runs inside the kernel acceptance path. It
certifies the scheduler-facing vCPU state transitions and concurrent multi-VM
control plane. A future profile will replace the bounded guest-SMP execution
model with independently executing secondary guest CPU instruction streams.
