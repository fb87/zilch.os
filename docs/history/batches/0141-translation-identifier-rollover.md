# Batch 0141 — translation identifier rollover

## Scope

Complete MEM-021 and HYP-013, and add the VMID rollover portion of TST-021.

## Implementation

- Added generation-tagged bounded ASID allocation.
- Added generation-tagged VMID allocation.
- Globally invalidate the relevant translation regime on namespace rollover.
- Lazily refresh stale live address spaces and VMs before execution.
- Ignore stale-generation release attempts against current allocation bits.

## Evidence

- `asid_rollover_reuse` forces stage-1 namespace rollover and refresh.
- `vmid_rollover_reuse` forces stage-2 namespace rollover and refresh.
- Real PL3 execution, real guest execution, four-CPU acceptance, and all
  lifecycle/reuse tests pass after rollover.
