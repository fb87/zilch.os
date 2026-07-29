# Userspace hypervisor API

`vcpu_run` returns a stable six-word result containing status, exit reason,
syndrome, fault address, guest PC, and exit qualification. Stage-2 faults carry
the reconstructed IPA in `qualification`; unknown hypercalls carry their call
number.

Pause, resume, and stop are capability-authorized lifecycle operations. Stage-2
mapping accepts a frame capability selector, never a caller-provided physical
address. Normal and device frames must match the requested memory type.

## Verification

ABI layout tests freeze the six-word result. ARM64 certification exercises real
guest exits, fault IPA reconstruction, lifecycle transitions, and negative
stage-2 permissions.
