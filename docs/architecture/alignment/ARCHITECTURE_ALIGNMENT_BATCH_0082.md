# Batch 0082 — architecture-alignment consolidation

This batch mechanically splits the production hypervisor into object, VMID, stage-2, vCPU, virtual IRQ, virtual timer, diagnostics, and lifecycle modules. Certification models remain under `tests/`, while the guest executable remains under `src/user/guests/`.

It also adds ABI layout verification, compatibility rules, and a requirement-to-evidence matrix. No new hypervisor capability is claimed. Real multi-vCPU and concurrent multi-VM execution remain open.
