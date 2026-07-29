# Batch 0176 — userspace control-plane graph

This batch replaces the inert production root task with a real management-domain
bootstrap graph. Root launches independently linked process, device, console,
domain, and supervisor instances at PL3, in documented dependency order, and
retains their process-bundle capabilities. Each service validates userspace-owned
quota, privilege, dependency, and restart policy before publishing a unique
health badge.

The certification build launches the same five ELF instances across four CPUs,
waits for the complete readiness mask, suspends and destroys every bundle, and
then requires all final kernel lifetime invariants to pass. Portable host tests
also exhaust every role and reject invalid roles.

This is a vertical control-plane foundation, not completion of section 7.
Memory-server production integration, general ELF/path loading, device and UART
protocols, VM management, crash status delivery, restart backoff, and guest OS
demonstrations remain open and are preserved as such in the readiness checklist.
