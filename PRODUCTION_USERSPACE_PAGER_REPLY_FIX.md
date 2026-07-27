# Production userspace pager reply fix

The one-shot reply token stores a logical thread ID and object generation. The pager control operations previously treated the logical thread ID as a global object-table ID, which fails for dynamically registered process threads whose object IDs are independently allocated. `fault_reply_sender` and `pager_reclaim_sender` now resolve the reply caller through the bounded thread table and validate the saved object generation before mapping or reclaiming memory.

The ARM64 bootstrap also maps the complete embedded userspace image and validates the same linked extent, preventing instruction faults when role-specific service entry code lies beyond the first page.
