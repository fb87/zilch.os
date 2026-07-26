# Memory-object manager

The Kernel Profile 1.0 foundation exposes bounded frame and page-table objects. Mapping is capability-mediated by callers, rejects W+X, updates the target address space and performs ASID invalidation through the architecture backend.
