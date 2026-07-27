# IPC badge protocol

An endpoint capability may carry a 64-bit badge. The kernel snapshots that
badge when it resolves write authority for a call. A receiver obtains the
snapshot in the ABI v1 `ipc_result.sender` word on either a direct rendezvous
or a queued receive.

The historical field name is frozen by ABI v1. Its value is capability
metadata, not a kernel thread ID. Kernel caller identity and generation remain
private in the one-shot reply authority and cannot be selected by userspace.

Calls accepted before their invoking capability is deleted or revoked retain
the captured badge. Later invocations through that capability fail normal
capability lookup. This defines one linearization point and prevents a queued
message from changing identity after acceptance.

Dynamically created tasks receive a minted endpoint derivation whose badge
combines the task thread's object generation and logical slot. This gives
userspace servers a stable identity for that process lifetime and prevents
destroy/reuse from inheriting the previous instance's server-side identity.
Rights remain attenuated independently of the badge.

Fault IPC uses the same capture and delivery rule. Replies do not traverse an
endpoint capability and therefore return a zero sender/badge word.
