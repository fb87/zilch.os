# Kernel lock ordering

Blocking kernel locks use increasing ranks: endpoint, IPC lifecycle, scheduler
timeout, capability authority, registry/mapping, CSpace, derivation, allocator,
translation identifier, and object table. Equal-rank multi-CSpace acquisition
uses increasing object address. Release is strict reverse order.

Certification tracks rank, identity, nesting depth, recursion, release order,
and maximum hold duration per CPU; any violation fails final acceptance.
