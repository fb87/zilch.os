# Memory-client capability deletion fix

The capability-control ABI takes the target task selector before the target
capability selector. Memory clients passed only the received frame selector,
so the kernel attempted to resolve that frame slot as a task capability and
returned `denied`.

Clients now pass task selector `0` (their own task) and the received frame slot
as the second argument before asking the server to release the underlying
frame.
