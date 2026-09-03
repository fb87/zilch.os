# Memory-server certification executable

The CONFIG_SELFTEST build of memory-server, built in place of
`src/user/servers/memory/main.cc`'s production request loop (see
`src/user/user.mk`'s conditional `USER_memory-server_SOURCE`). Acts as a
pager for genuine page faults delivered on the shared fault-delivery
endpoint: resolves two clients' data-abort faults with a scripted sequence
of invalid/read-only/correct `fault_reply_sender` calls, verifies an
instruction-abort fault terminates its sender, then serves the production
`memory_server_operation` API (via `sys::memory_service::service_request()`,
shared with the production build) to a pressure-test batch of clients before
exiting.
