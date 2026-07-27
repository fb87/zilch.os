# Batch 0134: production trace policy

This batch completes OBS-005, OBS-006, OBS-009, and OBS-010.

Versioned fixed records now cover exception, IRQ, scheduler, IPC, VM-exit, and user-fault paths. Routine events compile to no-ops in release, while fatal and console-contention capture remains available. Release console diagnostics redact guest registers and user/guest addresses; detailed diagnostics remain available in certification.

The clean four-CPU certification suite passes with zero failures. Both certification and release images build successfully, and a release ELF string audit confirms that guest-register and detailed user-fault formats are absent.
