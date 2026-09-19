# Hypervisor guest-visible architecture

Status: complete for the QEMU ARM64 1.0 platform profile, for guests of the
class the sample guest belongs to. What is *not* present is listed at the end,
because the gap between this and "boots Linux" is the substance of USR-030/031.

This is the guest's view. For the host-facing VMM ABI — the operations a VMM
calls to build, run and tear down a VM — see `abi/sys/v1/hypervisor.md`.

## Execution model

The guest runs at EL1/EL0 under stage-2 translation with the kernel at EL2.
Nothing about the host is visible in the guest's address space: stage-2 maps
only what the VMM explicitly mapped, from frame capabilities it holds, and
userspace cannot submit a physical address — `stage2_map` takes a guest IPA and
a frame capability selector, and the kernel requires the frame's type
(normal/device) to match the requested mapping type.

Stage-2 entries carry W^X, derived from the ELF section flags at load time. The
guest image is parsed and loaded entirely in userspace, at PL3, by
domain-manager; the kernel supplies mapping mechanism only and has no notion of
an executable format.

### Exits

`vcpu_run` returns to the VMM with one of:

| Reason | Cause |
| --- | --- |
| `none` | No exit condition |
| `hypercall` | Guest executed `HVC` |
| `wait` | Guest executed `WFI`/`WFE` |
| `stage2_fault` | Access to an IPA that is not mapped, or that violates its permissions |
| `system_register` | Trapped system-register access |
| `virtual_timer` | Virtual timer fired |
| `shutdown` | Guest requested power-off |

Alongside the reason the VMM receives the syndrome, fault address, guest PC and
a qualification word. For a stage-2 fault the qualification is the
reconstructed IPA; for a rejected guest hypercall it is the original call
number. `WFI`/`WFE` exits preserve ESR in the syndrome and advance the guest PC
before returning, so the VMM resumes past the instruction rather than
re-trapping it.

### System registers

PSTATE, SCTLR, TCR, CPACR, CNTKCTL and the translation base registers are
masked, aligned, emulated or rejected rather than passed through. A guest
access to a system register outside the supported set exits with
`system_register` and does not fault the host.

## What the guest sees

### Memory

Exactly what the manifest declares. The sample guest asks for 128 KiB of RAM
and nothing else. There is no firmware memory map, no e820 analogue, and no
device tree constructed for the guest — the guest is expected to know its own
layout, which is true of the embedded-RTOS class of guest and is not true of a
general-purpose kernel.

### Boot state

The VMM sets the initial stack pointer and PSTATE from the manifest and enters
at the image's entry point. There is no bootloader, no EL2 stub, and no PSCI
implementation: there is no way for a guest to bring up a secondary CPU, so
guests are single-vCPU in practice.

### Console — an emulated PL011

The guest's UART is **trapped and emulated**, not passed through. The sample
manifest declares zero passthrough devices, so the PL011 IPA window is left
unmapped, every guest access to it takes a stage-2 fault, and domain-manager's
vPL011 model services it. Character I/O is forwarded to the console server over
the ordinary control-plane endpoint, which is what puts guest output on the
same console as everything else.

Only the register state a real PL011 driver touches is modelled — DR, FR
(RXFE/TXFF), IMSC (RXIM) and CR behave; the rest of the 4 KiB window is
write-and-discard, read-as-zero. That is a deliberate scope decision, taken by
reading the guest driver's source, not an accident of incompleteness.

Passthrough of the real UART is not merely unimplemented but wrong here: the
host's console server owns that device frame exclusively, and handing the same
hardware to a guest would collide with that ownership.

### Interrupts

The guest gets a vCPU-resident virtual GIC CPU interface, with its state
(`ICH_*`, including the list registers) saved and restored across exits.
`virtual_irq_inject` lets the VMM raise a virtual interrupt into the guest —
which is how an emulated device signals, and how the vPL011 delivers input.

There is **no distributor or redistributor emulation.** A guest that programs
`GICD_*`/`GICR_*` to configure routing, priorities or enables will not find a
device there. Guests that work are those driven entirely through the CPU
interface with interrupts the VMM injects.

### Timer

The guest gets the architectural virtual timer. Expiry exits with
`virtual_timer` and the VMM decides what to do; the host kernel keeps the
physical timer for its own scheduling and does not surrender it.

## What is absent

Stated plainly because it bounds which guests can run, and because the
readiness checklist treats "a guest boots" and "Linux boots" as very different
claims (USR-030/031):

- **PSCI for guests.** PSCI appears in this tree only as a *consumer*: the
  host issues `CPU_ON` via SMC to firmware to start its own secondary CPUs
  (`platform/firmware.hh`). Nothing *provides* PSCI to a guest, so a guest
  `HVC` requesting `CPU_ON` is a rejected hypercall, and there is no secondary
  vCPU bringup and no SMP guest.
- **GIC distributor/redistributor.** See above.
- **A constructed device tree.** The guest is handed no description of its own
  machine; it must be built knowing it.
- **A root filesystem or block device model.** The guest has no storage.
- **Virtio devices of any kind.** The host uses virtio-mmio for its own block
  device; none is presented to a guest.

Each is a prerequisite for a general-purpose guest kernel, and each is real
work rather than configuration. The sample guest boots because the manifest
hands it precisely what it needs: memory, a PL011, the virtual timer, and one
forwarded interrupt.
