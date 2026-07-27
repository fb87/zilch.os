# Timer and scheduler deadline protocol

The kernel uses a per-CPU 100 Hz logical timebase. Each ARM64 CPU owns and
programs its local virtual timer; no CPU writes another CPU's timer register.

While a CPU has runnable userspace work, its next deadline is one logical tick
to preserve budget charging and preemption. When the CPU enters kernel idle,
the earliest generation-checked IPC or fault timeout becomes the next hardware
deadline. With no queued timeout, a bounded one-second housekeeping deadline
keeps boot and liveness diagnostics progressing.

A one-shot interrupt advances the logical counter by the delta that was
actually programmed. This prevents a 100-tick idle interval from appearing to
advance by only one tick. The timeout queue can therefore continue comparing
absolute logical deadlines without a whole-thread scan.

Frequency and arithmetic rules are fail closed:

- `CNTFRQ_EL0` must yield a nonzero 100 Hz interval within `CNTV_TVAL_EL0`.
- Oversized hardware intervals are split by clamping to the maximum supported
  delta.
- Zero or already-due deadlines program the minimum one-tick interval.
- IPC, fault, and scheduling deadlines saturate at `UINT64_MAX` rather than
  wrapping into the past.
- Logical counters also saturate at `UINT64_MAX`; an uptime wrap cannot make
  an expired deadline appear to be in the future.

Certification validates the live counter frequency and interval model on every
boot. Existing timeout ordering, pager-death, SMP timer, and root-only
acceptance tests exercise the integrated path.
