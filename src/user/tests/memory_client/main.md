# Memory pressure client

Certification-only PL3 client for the memory-server IPC protocol. Three clients
run on separate CPUs and repeatedly allocate, query, and release disjoint frame
handles through synchronous IPC before reporting completion by notification.
