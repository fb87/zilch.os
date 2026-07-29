# Batch 0178 — capability-isolated control-plane health IPC

Production root now creates one endpoint object for each process, device,
console, domain, and supervisor role. It delegates selector 11 only into the
matching task, using attenuated read/write authority and a role-specific badge.
After the complete readiness mask arrives, root continuously issues synchronous
health RPCs and validates both the protocol magic and responding role.

The versioned ABI also exposes a `describe` operation for dependencies, memory
quota, and restart limit. Unknown operations fail with `invalid_argument`.

This work found and fixed a production selector collision: the original graph
used root slots 16–30, overlapping bootstrap VM/vCPU selectors 28–29. Process
bundles now occupy slots 33–50 and private endpoints 51–55, with a compile-time
capacity assertion.

ARM64 certification creates five real endpoint objects, delegates them into five
independent PL3 tasks across four CPUs, validates every health reply, destroys
all task bundles and endpoints, and finishes with zero lifecycle failures.
Exit-status delivery and restart/backoff remain open.
