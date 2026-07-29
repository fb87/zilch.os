# Batch 0168: IPC completion gate

This batch supplies the missing end-to-end evidence required to close the
bounded Core IPC production gate.

The capability-transfer process pair first transfers two notification
capabilities atomically in a call. The server transfers both capabilities back
in its reply, and the caller proves both returned authorities are usable. The
same pair then forces a later two-capability transfer to fail after its first
mint, while the root verifies that rollback left no first destination behind.

Each of the three memory clients also requests a one-page OOL frame grant. The
server replies through the production OOL API. The client validates the
offset/length metadata, maps the received frame with normal inner-shareable
attributes, writes and reads a client-specific pattern, unmaps the page,
deletes its capability, and asks the server to destroy the frame.

Certification publishes dedicated `ipc_capability_batch`,
`ipc_ool_frame_grant`, and `ipc_completion_gate` results. The completion result
requires lifecycle races, successful and rollback capability batches, object
lookup/destroy races, OOL service transport, and dynamic endpoint lifecycle to
all pass. Final kernel acceptance separately requires endpoint, object, lock,
mapping, notification, process, and IPC latency invariants.

The release instruction check is a conservative static footprint count of the
complete ARM64 call, receive, and reply function bodies. It therefore bounds
the in-function fast path while runtime latency certification covers invoked
helpers and scheduling effects.
