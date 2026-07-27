# Memory pressure client

Certification-only PL3 client for the memory-server IPC protocol. Three clients
run on separate CPUs and repeatedly allocate, query, and release disjoint frame
handles through synchronous IPC before reporting completion by notification.

## Capability-transfer coverage

Each client first requests delivery into its occupied endpoint slot and expects
an atomic `busy` rollback. Normal cycles receive frame capabilities in slots
20–23, delete those capabilities, and then release the corresponding server
handles.
