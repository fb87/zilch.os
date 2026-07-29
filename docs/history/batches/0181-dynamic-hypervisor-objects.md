# Batch 0181 — dynamic hypervisor objects

The public hypervisor ABI now creates and destroys dynamic VM and vCPU objects.
The production pools contain four VM slots and eight vCPU slots. Each VM gets a
generation-tagged VMID, a dedicated page-aligned scrubbed stage-2 root, a
generation-checked object-table entry, and a caller-selected capability.
Each vCPU records its exact VM object reference and increments a bounded parent
child count.

VM destruction fails while execution, mappings, or child vCPUs remain. vCPU
destruction fails while running. Successful destruction revokes all
capabilities, unregisters the exact generation, waits for object readers,
releases VMID ownership, scrubs architectural/interrupt/timer/exit state and
root storage, then returns the slot to the pool.

PL3 certification reports VM create, vCPU create, parent-busy rejection, vCPU
destroy, stale-vCPU denial, VM destroy, stale-VM denial, and generation-safe
reuse independently. All eight lifecycle stages pass. Dynamic lower-level
stage-2 table population and race-safe run-versus-destroy serialization remain
open.
