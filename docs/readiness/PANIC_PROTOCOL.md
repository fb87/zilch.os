# Panic protocol

Fatal kernel exceptions and kernel-stack corruption enter
`kernel::panic::stop`. The path masks debug, SError, IRQ, and FIQ exceptions,
then writes only to the lock-free per-CPU emergency ring and the checksummed
`.noinit` crash record. It executes a full-system barrier and parks the CPU.

The panic implementation does not inspect the current thread, scheduler run
queue, capability state, allocator, object table, console lock, or any other
blocking kernel lock. It deliberately performs no formatted console output:
post-crash tooling consumes the preserved record after reboot, and emergency
records retain the immediate context.

Certification poisons the current user-thread index and holds the printk lock
while invoking the non-halting capture primitive. It then restores live state
and verifies the complete checksummed record. This demonstrates that crash
capture remains functional when scheduler and console state cannot be trusted.

The testable capture primitive and the non-returning stop primitive share the
same recording code, so fatal exception entry cannot diverge from the certified
record format.
