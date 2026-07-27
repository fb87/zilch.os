# Task and CSpace

A task is the protection-domain object. It owns a capability space and names
its address space. Threads execute within a task and resolve all kernel-object
authority through the task CSpace.

The current bring-up configuration creates one task per fuzz thread. A later root-task
milestone will allow multiple threads to share one task and address space.
