# Control syscall

Syscall 1 is the capability-authorized control plane for Kernel Profile 1.0.
It exposes capability lifecycle, thread control, frame mapping, notifications,
interrupt binding, and scheduling-context configuration. Object selectors are
always resolved through the caller's Task CSpace with type, rights, and object
generation validation.

The IPC fast path remains syscall 0.
