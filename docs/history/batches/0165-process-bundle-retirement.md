# Batch 0165: Transactional process-bundle retirement

This batch extends the kernel lifetime protocol across the complete bounded
process bundle.

Process destruction first quiesces the target thread and retires its delegated
memory resource. It then enters one capability-authority transaction,
revalidates the exact thread, task, and address-space control capabilities,
publishes the terminated state, removes every address-space mapping, drains
the task CSpace and its mapping attachments, and revokes the thread, task,
address-space, and scheduling-context references.

Only after authority is released does destruction unregister those objects and
wait for pre-existing object readers. The cleared scheduler slot is therefore
not reusable while capabilities, mappings, or readers from the prior
generation remain live.

Final certification now checks the ownership and lifecycle relationship among
every bounded thread, task, address space, and scheduling context, in addition
to the existing mapping, object, lock, IPC, notification, and interrupt
invariants. The ARM64 four-CPU certification workload and final acceptance
pass with this transaction.
