# Root service graph

The production root launcher creates the five core service bundles in dependency
order and supervises their readiness/failure notification mask. It creates a
distinct endpoint object for each role, delegates that endpoint only to the
matching task, and continuously performs synchronous health RPCs after startup.
Selectors occupy a compile-time checked range disjoint from bootstrap
hypervisor capabilities. Restart state remains tracked by the remaining
userspace readiness requirements.
