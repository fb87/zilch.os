# Module: IPC syscall dispatch

Implements the kernel-side minimal `sys_ipc` dispatch. Architecture-specific register extraction and syscall-entry recognition are delegated to `sys::arch::syscall`.
