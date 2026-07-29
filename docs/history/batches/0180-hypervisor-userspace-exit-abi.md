# Batch 0180 — hypervisor userspace exit and mapping ABI

The userspace hypervisor API now returns the complete six-word vCPU exit record:
status, reason, syndrome, fault address, guest PC, and qualification. This closes
the previous information loss where EL2 reconstructed a stage-2 fault IPA or
captured an unknown hypercall number but the control syscall discarded it.

Pause, resume, and stop now invoke the production lifecycle state machine rather
than directly assigning a stopped state. The stage-2 mapping syscall accepts a
frame capability selector instead of a caller-selected physical address. It
requires read authority, validates normal versus device frame type, and rejects
empty, write-only, W+X, executable-device, and unknown permission bits.

ABI layout fixes the exit result at six words. Four-CPU ARM64 certification
passes real guest execution, fault reconstruction, WFI/timer exits, lifecycle
tests, new permission negatives, and all final invariants. Dynamic userspace
VM/vCPU creation and destruction remain open.
