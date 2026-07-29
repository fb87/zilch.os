# Userspace control-plane policy

`service_policy` is the bounded startup contract owned by userspace. It records
dependency, restart, privilege, and memory-quota policy for every core service.
The kernel sees only an image role and capabilities delegated by root.
