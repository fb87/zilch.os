# Zilch Hypervisor Profile 0.1 Candidate

## Scope

This candidate establishes the capability-authorized virtualization control plane
without weakening the certified ARM64 kernel Profile 1.0 host.

Implemented checkpoints:

- **HV-A:** generation-checked `VirtualMachine` and `VirtualCpu` objects,
  root CSpace capabilities, VMID allocation, and lifecycle state.
- **HV-B:** bounded stage-2 mapping model with alignment, overlap, IPA-range,
  and W^X validation plus EL2 stage-2 invalidation hooks.
- **HV-C:** complete first-profile vCPU context, single-run ownership,
  virtual IRQ pending state, and structured exit records.
- **HV-DIAG:** deterministic boot and userspace self-tests, negative capability
  fuzzing, and detailed failure records.

The actual EL2 guest-entry/exit assembly bridge deliberately returns
`unsupported` in this candidate. It is the next runtime gate. This prevents an
incomplete `eret` path from corrupting the certified host.

## Root capabilities

The root task receives:

- slot 28: VM capability (`read|write|grant|control`)
- slot 29: vCPU capability (`read|write|control`)

All hypervisor operations resolve through CSpace type, rights, and generation
checks. No raw VM ID, vCPU ID, or kernel pointer grants authority.

## Diagnostics

A VM diagnostic record contains:

- checkpoint number
- error result
- VM and vCPU generation
- VMID
- IPA and operation value
- syndrome, fault address, and guest PC

Failures print a compact record such as:

```text
[TEST] expected-error operation=stage2_overlap expected=-5 actual=-5 result=PASS ipa=0 value=0
```

The guest-exit path reports:

```text
[ERR] hv guest-exit result=... reason=... esr=... far=... pc=... vmid=... run=...
```

## Expected boot records

```text
[HV-A] objects-capabilities result=PASS vmid=1
[HV-B] stage2-map-unmap result=PASS operations=9
[HV-C] vcpu-state-machine result=PASS irq=27
[HV-DIAG] profile=0.1 self-test=PASS operations=9 failures=0
[TEST] name=hypervisor_profile_0_1 result=PASS
[TEST] name=hypervisor_negative_fuzz result=PASS
```

## Remaining runtime gate

Hypervisor Profile 0.1 is not complete until the following are implemented and
validated:

1. EL2 guest-entry and exit assembly bridge.
2. Real stage-2 translation tables rooted by `VTTBR_EL2`.
3. A separately linked Zilch guest image in guest RAM.
4. Hypercall console and shutdown.
5. Virtual timer and interrupt injection.
6. Guest EL0 acceptance and VM/vCPU teardown fuzzing.
