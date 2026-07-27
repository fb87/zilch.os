# Batch 0120: Memory-service selector layout

The root certification CSpace previously assigned the memory server selectors
`17/20/23` and used bases `18/21/24` for three clients. Client 2 therefore
aliased the server task selector `20` and server address-space selector `23`.
Protocol execution could complete, but teardown resolved capabilities for the
wrong bundle and reported `client-teardown stage=9`.

The service graph now owns a disjoint range:

- memory server: thread/task/space `40/41/42`;
- clients: threads `43-45`, tasks `46-48`, spaces `49-51`.

Compile-time checks keep the ranges ordered and below the memory test selector
at `54`. No production ABI or IPC semantics changed.
