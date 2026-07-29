# Batch 0177 — production memory-service integration

The independently linked memory-server ELF now has a production request loop
instead of executing only the certification pager scenario. Production root
launches it before the process, device, console, domain, and supervisor roles,
retains its process-bundle authority, and requires its distinct readiness bit
before treating the management graph as healthy.

The production loop accepts bounded allocate, grant, release, query, and
shutdown requests through the versioned memory IPC ABI. Capability-transfer
failure reclaims the resource-backed frame before reporting the error. The
existing certification-only pager and three-client pressure workload remains
behind `CONFIG_SELFTEST`.

ARM64 release boot remains resident without a user fault through the bounded
smoke interval. Inventory import, asynchronous request queues, scalable handle
storage, exit-status delivery, and restart/backoff remain open.
