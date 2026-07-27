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

Map/resume validates against the immutable pending fault record before changing
the address space. The requested mapping must cover the recorded fault page.
A data-abort write requires writable permission; a data-abort read requires
readable permission; an instruction abort requires executable permission.
Alignment, invalid-context, wrong-page, malformed-permission, and
access-incompatible replies are rejected. Rejection leaves the thread blocked,
does not install a mapping, and preserves pager reply authority for a corrected
retry or terminate response.

Certification executes two real PL3 translation faults and verifies mapping,
resume, access, reclaim, and teardown. The pager verifies data-abort kind,
nonzero ESR, exact fault address, and nonzero PC. It then executes an ARM64
`udf #0` instruction from a separately created PL3 process and verifies
instruction-abort kind, nonzero ESR, and nonzero PC before terminating that
process. The pager continues serving the memory-service graph, proving the
metadata comes from production exception entry rather than a kernel-side
model.

The first real data-fault resolution deliberately submits a different page and
then a read-only mapping for a write fault. Both fail with deterministic errors
before the valid read-write retry resumes the client. The kernel certification
fixture independently verifies unchanged blocked state, pending disposition,
and zero mappings after both failures.
