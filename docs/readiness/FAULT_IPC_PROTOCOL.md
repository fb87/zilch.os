# Fault IPC protocol

User exceptions are classified in the architecture exception path and
published to the task's configured pager endpoint. The four message words
contain the requested disposition placeholder, public ABI v1 fault kind,
fault address, and faulting instruction pointer. Reply authority privately
binds the pager response to the faulting thread and object generation.

For recoverable data faults, the pager allocates a frame and uses
`fault_reply_sender` to map the page and resume the caller. For faults that
userspace policy rejects, the pager replies with
`fault_disposition::terminate`; the kernel consumes the one-shot reply
authority and transitions only that caller to terminated.

Certification executes two real PL3 translation faults and verifies mapping,
resume, access, reclaim, and teardown. It then executes an ARM64 `udf #0`
instruction from a separately created PL3 process. The pager verifies
`fault_kind::instruction_abort` and a nonzero faulting PC, terminates that
process, and continues serving the memory-service graph. This proves the
instruction-fault path uses production exception entry and userspace policy,
not a kernel-side model.
