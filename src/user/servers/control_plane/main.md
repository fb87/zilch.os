# Core control-plane service image

This independently linked PL3 image supplies the process, device, console,
domain, and supervisor roles. Each instance validates role-owned policy,
publishes a health badge, and remains schedulable for subsequent IPC protocol
attachment. The root task owns launch order and process capabilities.
