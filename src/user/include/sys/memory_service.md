# Module: memory_service

## Purpose

Shared frame-allocation request handling for the memory-server program,
used identically by both its production build
(`src/user/servers/memory/main.cc`) and its CONFIG_SELFTEST pager-fault
certification build (`src/user/tests/memory_server_certification/main.cc`).
Extracting `service_request()` here keeps that one piece of real request
logic in a single place instead of duplicating it across the two `main()`
bodies user.mk's `USER_memory-server_SOURCE` chooses between.

## Responsibilities

- `service_request()`: decodes a `memory_server_operation` (allocate_frame/
  grant_frame/release_frame/query/shutdown) against the caller's handle
  table and issues the corresponding `resource_frame_create`/`frame_destroy`/
  `memory_resource_query` control calls.
- `handles[]`/`handle_state`: per-handle ownership tracking shared by both
  callers.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- `handles[]` has external linkage only in the sense that each of the two
  programs links its own copy -- they are never linked together, so there is
  no ODR concern between them despite both defining this same `inline`
  storage.

## Verification

Exercised by both `src/user/servers/memory/main.cc`'s production request
loop and `src/user/tests/memory_server_certification/main.cc`'s
`memory_server_protocol`/`memory_completion_gate` certification checks.
