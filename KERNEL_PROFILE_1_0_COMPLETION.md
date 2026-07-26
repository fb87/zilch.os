# ARM64 Kernel Profile 1.0 completion batch

The completion batch defines the kernel contract as complete when all core
objects are invokable through capabilities, the root task is the only initial
policy authority, faults are delivered through IPC, mappings are pager
controlled, interrupts target notifications, scheduler parameters are carried
by scheduling contexts, and lifecycle operations reject stale generations.

This source implements the complete invocation surface and keeps the known-good
compatibility SMP fuzz image as the runtime acceptance workload. The next
runtime transition is to replace that compatibility image with a separately
linked root-server ELF without changing the kernel ABI.
