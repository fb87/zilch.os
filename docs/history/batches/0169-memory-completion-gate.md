# Batch 0169: Physical memory and address-space completion gate

The ARM64 certification launcher now asks QEMU to generate the exact virt
machine DTB used for the run and loads it at the platform firmware-probe
address. The kernel parses that DTB, imports its RAM ranges and reservation
records, reserves the DTB blob and kernel image, and publishes only the
remaining page-aligned extents. Final acceptance fails if initialization used
the static platform fallback.

The observed certification inventory contains two allocatable regions because
the dynamically loaded DTB splits the usable RAM range. This exercises
discontiguous allocator publication and proves the firmware blob itself is not
delegated.

The new userspace `memory_completion_gate` requires:

- pager data-fault resume and instruction-fault containment;
- OOL frame-grant mapping and release;
- frame and page-table lifecycle accounting;
- reverse-mapping database and capability-authority revoke;
- normal/device attribute rejection and pressure recovery;
- nested resource delegation and balanced extent return;
- extent retype, metadata exhaustion/reuse, and injected rollback;
- four-CPU fuzz, process teardown, and object-generation reuse.

Final kernel acceptance independently requires discovered firmware inventory,
mapping database integrity, object accounting, lock order, endpoint,
notification, interrupt, process-lifecycle, and latency invariants.

The production scope is deliberately bounded: DTB is the ARM64 firmware
format, allocator/resource/mapping tables have fixed certified capacities,
notifications are nonblocking, pager policy is per-task with fault-page
validation, and orphaned faults terminate on the bounded safety deadline.
