# Batch 0182 — Hypervisor core completion

This batch closes the Hypervisor core readiness gate with executable dynamic
stage-2 tables, complete bounded vCPU state access and sanitization, serialized
VM lifetime transactions, access/dirty tracking, a userspace VMM layer, MMIO
and system-register exits, deadline-driven virtual timers, and a virtual GIC
with ARM list-register acceleration plus an equivalent software fallback.

Certification no longer treats the SMP execution model as completion evidence.
A runtime EL2 harness sends work to all four physical CPUs. Four independent
guest instruction streams execute concurrently, complete a guest-side atomic
barrier, take virtual interrupts, and resume after every vCPU migrates to a
different physical CPU. A second test concurrently executes two VMs with
independent stage-2 roots and barriers, then faults one VM at an unmapped
instruction address and verifies the peer VM remains runnable.

Runtime evidence includes:

`[HV-REAL-SMP] physical-cpus=4 guest-streams=4 barrier=4 irqs=4 migrations=4 result=PASS`

`[HV-REAL-MULTIVM] vms=2 vcpus=4 barriers=2,2 crash-isolation=PASS result=PASS`
