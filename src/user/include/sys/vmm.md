# Userspace VMM

`sys::vmm::machine` is the production userspace orchestration layer for the
capability-authorized hypervisor ABI. It owns VM/vCPU selectors, frame-backed
stage-2 loading, architectural configuration, bounded exit dispatch, virtual
interrupt injection, unloading, and ordered teardown. Policy remains in
userspace; physical addresses and private kernel objects never cross this API.
