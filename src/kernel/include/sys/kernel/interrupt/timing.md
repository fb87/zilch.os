# IRQ-disabled duration

Scoped kernel critical sections disable IRQ delivery through
`interrupt::timing::save_and_disable()` and restore the complete architectural
IRQ state through `interrupt::timing::restore()`. The wrapper timestamps the
masked interval with the architectural counter and retains per-CPU sample
counts and maximum durations.

The currently scoped sections are serialized kernel logging and scheduler
timeout-queue mutation. Boot-to-user exception transitions use architectural
exception state rather than this scoped API and are measured separately by the
interrupt/preemption latency requirements.

The same telemetry records IRQ handler service, timer-driven preemption
service, cross-CPU wake request-to-IPI receipt, and IPC syscall service.
Certification reports all maxima and sample totals after hypervisor execution,
pager and memory services, lifecycle races, and SMP fuzzing.

The ten-millisecond values compiled into the QEMU profile are diagnostic
references, not certification limits. Repeated identical QEMU runs showed
large elapsed-time variation when the host descheduled a vCPU. Functional
acceptance therefore does not depend on those references. Closing SCH-017 and
SCH-020 through SCH-023 requires stable limits and retained evidence on the
supported real-hardware platform.

AMD64 retains the telemetry mechanism for build compatibility, but its runtime
counter frequency is not yet calibrated and AMD64 remains compile-only.
