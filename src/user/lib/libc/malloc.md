# Heap

First-fit allocator over pages the process maps for itself. There is no
`brk` and no `mmap`: growing the heap means creating a frame, charged
against this task's page quota, and mapping it at the next address above the
heap base.

So the heap is bounded by the quota rather than by the machine. A task that
exhausts its quota gets null from `malloc` instead of taking memory from
anything else, which is the property that makes a bounded per-task quota
meaningful in the first place. Blocks carry a small header and live in one
address-ordered list.
