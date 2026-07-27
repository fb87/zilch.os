# ARM64 Root-only boot certification

> **Historical status notice:** This file records the state of its named batch. The current program status, completed follow-on work, remaining requirements, and delivery order are maintained in [`PROGRAM_CHECKLIST.md`](PROGRAM_CHECKLIST.md).

`BOOT_PROFILE=root` is the default. The kernel maps the flat payload generated
from `init.elf` as the sole initial EL0 task at `0x20000000`. The root task uses
only the capability-authorized control ABI and emits machine-readable records.

The previous deterministic SMP workload remains available with:

```sh
make ARCH=arm64 PLATFORM=qemu-arm64-virt BOOT_PROFILE=compat run
```

A root-only pass proves the loader, initial Task/CSpace/AddressSpace/Thread, and
control ABI. Dynamic userspace creation of the full SMP workload remains the
next certification gate; this patch does not claim that kernel-created object
pools have been fully replaced.
