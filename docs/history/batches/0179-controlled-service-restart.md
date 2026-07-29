# Batch 0179 — controlled service restart

The control-plane ABI now includes an orderly `stop` operation. A service first
replies to the management call and then uses the atomic kernel exit transition
to publish a collision-free role lifecycle badge. Userspace policy exposes a
bounded `may_restart` decision and rejects attempts at or above each role's
limit.

Certification stops the live process service through its private endpoint,
waits for its exit badge, destroys the old process bundle, recreates the same
role, remints its existing endpoint into the replacement CSpace, waits for fresh
readiness, and validates a new health RPC. It then tears down the complete graph
and requires all kernel lifetime invariants to pass.

This is controlled restart, not unexpected-crash recovery. Fault-to-supervisor
status delivery, retry backoff, and time-windowed crash-loop accounting remain
open.
