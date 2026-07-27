# Batch 0114: PL3 memory-server protocol and multi-CPU pressure

Adds a public request/reply protocol for the userspace memory server and a
certification-only client executable. Three PL3 clients run on CPUs 1-3 and
perform 64 cycles of four frame allocations, resource queries, and releases
through synchronous IPC. Handles are owner-bound by sender identity, and the
server refuses foreign release or shutdown while handles remain allocated.

This is bounded integration evidence, not completion of the production memory
server. Capability transfer of allocated frames, asynchronous requests,
back-pressure, cancellation, and general client policy remain open.
