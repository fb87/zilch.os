# Production independent service images batch

This increment removes the certification pager server and client entry points
from the root task image. The userspace build now emits three independently
linked programs at the common PL3 virtual base:

- `init.elf` — root task and acceptance coordinator;
- `memory-server.elf` — pager and frame-owner service;
- `pager-client.elf` — reusable faulting client image.

For the ARM64 QEMU `virt` production target, the three flat program images are
embedded in the kernel bundle. Dynamic process creation selects the mapped text
image from the requested bootstrap role. Every process retains a private stack
and address-space page table; pager clients therefore use the same fault virtual
address without sharing mappings.

The userspace pager certification now creates one memory server and two
successive pager-client processes. It destroys and recreates the client bundle
between runs, verifies role-selected image reuse, maps distinct frames, reclaims
each mapping, and requires separate completion badges.

This is a controlled bootstrap image registry, not yet a general runtime ELF
loader. The next loader increment should consume validated ELF program headers
from earlyfs and remove role-to-image policy from the architecture backend.

## Runtime verification update

The ARM64/QEMU four-CPU certification run has verified both independently
linked pager clients and the separately linked memory server. The runtime emits
two delivered user faults followed by:

```text
[TEST] name=userspace_pager_service result=PASS
[TEST] name=dynamic_ipc_objects result=PASS
[TEST] name=root_created_objects result=PASS
```

The reply-before-notify correction is part of the verified protocol: the server
replies to the client's completion call before notifying root that process
teardown and slot reuse are safe.
