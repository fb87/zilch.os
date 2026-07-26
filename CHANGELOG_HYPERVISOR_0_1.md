# Hypervisor Profile 0.1 changelog

## Candidate 0.1-a

- Added versioned hypervisor ABI operations and exit reasons.
- Added VM and vCPU objects to the capability-authorized control path.
- Added root VM/vCPU capabilities in CSpace slots 28 and 29.
- Added VMID allocation and EL2 host configuration HVC.
- Added EL2 stage-2 invalidation HVC.
- Added bounded stage-2 mapping metadata and W^X/overlap/alignment checks.
- Added full first-profile vCPU context and structured exit records.
- Added virtual IRQ pending state and single-run ownership.
- Added boot-time HV-A/HV-B/HV-C diagnostics.
- Added 4,096-operation userspace negative capability fuzz test.
- Kept actual guest entry disabled until its assembly bridge is complete.

## Guest execution candidate

- Added a real EL2 guest entry/exit bridge with per-CPU host-frame preservation.
- Added 64 KiB private guest RAM and executable three-level stage-2 tables.
- Added an embedded guest EL1 payload with console, time query, and shutdown HVCs.
- Added structured unexpected-exit and guest-register diagnostics.
- Fixed the parallel build dependency between the embedded root payload and the ARM64 boot object.
