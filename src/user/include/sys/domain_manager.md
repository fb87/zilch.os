# Domain-manager lifecycle API

`sys::domain_manager::manager` is the stable userspace lifecycle wrapper around
`sys::vmm::machine`. It records domain state while delegating capability-based
VM/vCPU creation, stage-2 frame mapping, architectural configuration, execution,
pause/resume/stop, and ordered teardown to the VMM layer.

The wrapper contains no image-selection or device-assignment policy. Those
decisions belong to the PL3 domain service and its management protocol.
