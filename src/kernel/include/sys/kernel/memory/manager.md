# Memory-object manager

The kernel bootstrap foundation exposes bounded frame and page-table objects. Mapping is capability-mediated by callers, rejects W+X, updates the target address space and performs ASID invalidation through the architecture backend.
