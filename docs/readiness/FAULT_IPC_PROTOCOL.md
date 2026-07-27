# Fault IPC protocol

User exceptions are classified in the architecture exception path and
published to the task's configured pager endpoint. ABI v1 freezes the four
message words as `fault_message`: public fault kind, raw architecture syndrome,
fault address, and faulting instruction pointer, in that order. Size,
alignment, offsets, enum widths, and selected values are compile-time checked.
Reply authority privately binds the pager response to the faulting thread and
object generation.

For recoverable data faults, the pager allocates a frame and uses
`fault_reply_sender` to map the page and resume the caller. For faults that
userspace policy rejects, the pager replies with
`fault_disposition::terminate`; the kernel consumes the one-shot reply
authority and transitions only that caller to terminated.

Certification executes two real PL3 translation faults and verifies mapping,
resume, access, reclaim, and teardown. The pager verifies data-abort kind,
nonzero ESR, exact fault address, and nonzero PC. It then executes an ARM64
`udf #0` instruction from a separately created PL3 process and verifies
instruction-abort kind, nonzero ESR, and nonzero PC before terminating that
process. The pager continues serving the memory-service graph, proving the
metadata comes from production exception entry rather than a kernel-side
model.
