# Memory-server completion badge aggregation fix

Batch: 0116

The memory-server capability-transfer workload changed relative client timing.
The root certification runner previously waited for client badges sequentially
and discarded valid badges for later clients when they arrived early.

The wait helper now accumulates notification bits and completes only when the
full expected mask is present. High-order failure badges still fail immediately.
No kernel IPC or capability-transfer semantics changed.
