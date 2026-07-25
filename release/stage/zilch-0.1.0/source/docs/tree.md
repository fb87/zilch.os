# Source tree

- `include/sys/abi/v1`: stable native kernel/userspace ABI.
- `include/sys/arch/v1`: versioned kernel/architecture contract.
- `include/sys/platform/v1`: versioned kernel/platform contract.
- `kernel`: generic L4 mechanisms only.
- `arch`: ARM64 and AMD64 CPU backends.
- `platform`: QEMU virt and Q35 machine backends.
- `user`: libraries, root task, servers, drivers, personalities, domains, apps.
- `runtime`: userspace CRT, entry code, and linker scripts.
- `image`: earlyfs manifests and image composition tools.
- `tools`: host-side development and traceability tools.
- `tests`: compile, boot, userspace, isolation, hypervisor, and safety tests.
