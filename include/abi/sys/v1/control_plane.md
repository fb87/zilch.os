# Control-plane service ABI

The root task uses stable role identifiers to launch the independently linked
process, device, console, domain, and supervisor services. Each service
publishes one readiness bit through the root notification after validating its
bounded startup policy. Role identifiers are image-selection metadata, not
kernel policy: lifecycle and restart decisions remain in the root task.

Each role also receives a private capability-protected endpoint. `health`
returns the protocol magic and exact role; `describe` returns the dependency
mask, quota, and restart limit. `stop` replies before atomically exiting and
publishing a role-specific lifecycle badge. Unknown operations return
`invalid_argument`.
