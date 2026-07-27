# 0115 — Reply Capability Transfer and Client-Owned Frames

## Scope

This batch extends the existing single-capability IPC transfer mechanism to
reply and reply-receive operations. The PL3 memory server uses the mechanism to
deliver a derived frame capability into a CSpace slot selected by each client.

## Atomic reply semantics

The kernel validates and mints the reply transfer before consuming the
single-use reply authority. When the destination is invalid or occupied, the
reply remains live and the caller remains blocked. The server can therefore
roll back its allocation and send a second error reply without racing caller
teardown.

## Memory-server lifecycle

A successful allocation now creates a resource-backed frame in the memory
server, transfers read/write authority to the requested client slot, and keeps
a server handle for controlled release. The client deletes the received
capability before requesting release. An occupied destination causes the server
to destroy the newly created frame and clear its handle before reporting the
error.

## Evidence and limits

Three clients exercise successful frame delivery and occupied-slot rollback.
The mechanism remains single-capability and bounded. General receive windows,
multi-capability atomic transfer, asynchronous queues, and long-duration race
fuzz remain open.
