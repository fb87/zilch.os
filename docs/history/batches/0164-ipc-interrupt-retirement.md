# Batch 0164: IPC and interrupt object retirement

This batch applies the kernel lifetime protocol to endpoint, notification, and
interrupt state.

Dynamic endpoint destroy now locks the endpoint before capability authority,
revalidates the exact control capability, rejects nonempty queues, and
publishes a retiring state before revocation. Call and receive recheck that
state after acquiring the endpoint lock, so an operation that resolved the
object just before revoke cannot enqueue afterward. The grace-period wait
occurs only after both locks are released.

Notification destroy revokes under capability authority and likewise releases
authority before unregister waits for pre-existing readers.

IRQ registration uses compare/exchange publication and rolls the registry
entry back if hardware configuration fails. Dispatch reads the registry with
acquire ordering. Binding masks the line before replacing notification state
and rejects an active interrupt rather than changing its target mid-delivery.

Final certification validates:

- endpoint ring bounds, empty retired endpoints, and dynamic object state;
- released notification object, waiter, and badge state;
- IRQ registry identity, delivery/acknowledgement monotonicity, active/masked
  consistency, storm masking, and live notification references.

The full four-CPU workload, dynamic IPC lifecycle, object reuse, IRQ
delegation/acknowledgement/storm tests, and final acceptance pass.
