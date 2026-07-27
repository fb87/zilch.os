# Batch 0131: user and guest fault containment

## Targets

- Complete HYP-047 guest abort records.
- Complete SEC-023 recoverable userspace fault handling.
- Complete SEC-024 guest-failure containment.

## Implementation

EL2 abort exits now combine HPFAR_EL2's fault IPA page number with FAR_EL2's page offset. Every guest abort record therefore carries the syndrome, fault address, guest PC, and reconstructed IPA.

The vCPU run boundary distinguishes recoverable stage-2 faults from fatal unexpected traps. Stage-2 faults return to the VMM with the vCPU runnable. Unexpected traps transition only the owning vCPU and VM to faulted state and retain the full diagnostic record.

User instruction and data faults continue through the existing pager endpoint protocol. Failed delivery faults only the current thread and schedules another runnable thread or the permanent kernel idle context.

## Evidence

Negative model checks validate abort classification, IPA reconstruction, and fatal-exit policy. The clean four-CPU certification run passes real guest execution, hypervisor negative fuzz, `fault_ipc_delivery`, two userspace pager recoveries, and final root-only acceptance with zero failures.
