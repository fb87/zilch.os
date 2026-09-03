# Memory server executable

The production memory-server request loop, built as `init.elf`'s memory
service (see `src/user/servers/memory/main.cc`). Serves the
`memory_server_operation` API (allocate_frame/grant_frame/release_frame/
query/shutdown) on `sys::native::service_endpoint` via
`sys::memory_service::service_request()`, shared with the CONFIG_SELFTEST
pager-certification build at
`src/user/tests/memory_server_certification/main.cc` (`src/user/user.mk`'s
conditional `USER_memory-server_SOURCE` chooses between the two).

## Reply capability delivery

Allocation requests include a receiver-selected destination CSpace slot. The
server replies with a derived read/write frame capability. If the destination
is occupied, the reply transfer fails without consuming reply authority; the
server destroys the frame, clears the handle, and reports the error in a retry
reply.
