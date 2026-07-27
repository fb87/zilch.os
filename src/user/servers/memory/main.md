# Memory server executable

The certification memory server is linked as an independent ELF image rather
than sharing the root task's text mapping. It owns two frames and services two
successive pager clients through endpoint selector 10. For each client it
receives a data-abort IPC, maps one frame with read/write permission, receives
the completion call, reclaims and destroys the frame, and signals the root
notification with a client-specific badge.

This executable is deliberately small. It verifies image isolation, pager reply
state, frame ownership, mapping teardown, and process-image reuse without making
the memory server part of the kernel.
