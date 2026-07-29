# Batch 0167: Bounded Core IPC completion

Core IPC now has one bounded transaction model across call, receive, reply,
reply-receive, cancellation, timeout, teardown, capability transfer, and
out-of-line payloads.

The transfer descriptor may reference a user batch of at most four capability
transfers. The kernel snapshots that batch before blocking, holds capability
authority across the complete mint sequence, rejects duplicate destinations,
and deletes every capability installed earlier in the sequence if any later
entry fails. Certification forces this partial-failure path during the
cross-CPU transfer/revoke workload.

Out-of-line messages use a transferred frame capability plus checked offset
and length metadata. Payloads are limited to one page. Receivers map the frame
under their own memory-resource policy, avoiding variable kernel buffers,
remote-address-space copies, and unbounded allocation in IPC paths.

Notifications are deliberately nonblocking badge accumulators. Atomic signal
coalescing and consume are the complete policy; endpoint IPC remains the sole
blocking primitive and therefore the sole owner of timeout and scheduling
donation. IRQ bindings retain generation-checked notification references.

Normal remote completion and thread teardown use receiver-targeted reschedule
SGIs. Final certification fails if measured IPC service latency has no samples
or exceeds the configured 10 ms architectural-counter limit. Release checking
also records upper bounds for the complete call, receive, and reply function
instruction footprints.
