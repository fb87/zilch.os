# User execution-state quiescence fix

Patch: 0088

The initial quiescence implementation set `thread::executing` when a thread was
selected for PL3 execution, but cleared it only from timer/IPI scheduling paths.
A thread entering the kernel through SVC or a user fault could block in IPC while
remaining marked as executing. Process destruction then correctly refused to
reclaim the address space with `error_t::busy`, causing the pager lifecycle and
all dependent certification tests to fail.

The ARM64 lower-PL synchronous exception path now:

1. saves the current PL3 frame and publishes `executing=false` on entry;
2. dispatches control IPC or fault handling;
3. publishes `executing=true` only when the selected return frame targets PL3;
4. leaves the flag false when the scheduler installs the EL1 idle frame.

The initial PL3 entry also publishes `executing=true` before `eret`.

This keeps teardown conservative while allowing blocked or terminated threads to
reach the quiescent state required for safe address-space reuse.
