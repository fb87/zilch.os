# virtio driver

Userspace virtio-mmio driver. Owns the one grantable virtio-mmio page and
serves `abi::v1::block_operation` over its own service endpoint.

## Transport window

`platform/qemu_arm64_virt`'s `virtio_mmio_base` is `0x0a000000`, with 32
`virtio,mmio` transports at `0x200` stride.

The single grantable page is the **last** one — `0x0a003000`, covering
transports 24..31 — not the first. QEMU populates these transports
*downward from the top*: `info qtree` with one `-device virtio-blk-device`
shows it on `virtio-mmio-bus.31`, i.e. `0x0a000000 + 31 * 0x200 =
0x0a003e00`. Granting the first page instead yields a window of slots that
all read valid magic with `DeviceID == 0`, which is exactly what this
driver's probe reported before the placement was pinned down.

The window and the interrupt numbering were confirmed against this
platform's real device tree (a `dumpdtb` of the same machine
`tools/run/run.sh` boots), not assumed: the nodes carry
`interrupts = <0 16 1>`..`<0 47 1>`, i.e. GIC SPI 16..47 = INTID 48..79,
flags 1 = **edge-rising**. Transport 31 therefore takes INTID 79, which the
probe confirms at runtime (`irq=0x4f`).

Edge-rising differs from `pl011@9000000`'s `<0 1 4>` (level-high) which the
serial driver relies on, and it changes the acknowledge path: an
edge-triggered line does not re-assert while a condition remains pending, so
a handler must re-check the device's `InterruptStatus` after acknowledging
rather than assuming another edge will arrive for work already queued.

## Transport version

`tools/run/run.sh` passes `-global virtio-mmio.force-legacy=false`. Without
it QEMU's virt board presents these transports as **legacy** (MMIO version
1) — confirmed by this driver's probe reading `version=0x1` — which uses an
entirely different queue setup (`QueuePFN`/`GuestPageSize`) than the modern
split `QueueDesc`/`QueueDriver`/`QueueDevice` address registers. With the
global set, the probe reads `version=0x2`.

## Why the driver probes instead of hardcoding a slot

Which transport QEMU plugs a given `-device` into is not answerable from the
device tree: all 32 nodes are present whether or not a device is attached,
and an unpopulated slot still reads the `virt` magic and a valid version.
Only `DeviceID` distinguishes an empty slot (`0`) from a block device (`2`).

This is also why the driver holds **no interrupt capability yet**. The IRQ to
request is `48 + slot`, which is unknown until the probe answers; the kernel
has only `dynamic_interrupt_count == 16` interrupt objects, so speculatively
creating one per candidate slot would cost half the pool on a guess. Root
therefore creates only the MMIO device frame (`create_block_resources()`),
and the interrupt capability follows once the slot is known.

## Capabilities

Root creates and mints these before the process runs
(`root_graph.hh`'s `create_block_resources()` / `mint_block_resources()`):

| Slot | Capability                                    |
|------|-----------------------------------------------|
| 3    | own address space                             |
| 11   | own service endpoint                          |
| 14   | root notification (ready / failure badge)     |
| 20   | virtio-mmio device frame                      |
| 23   | serial-driver service endpoint (diagnostics)  |

Bring-up diagnostics are written as text through slot 23 — the same endpoint
console-server forwards to — so a probe scan is observable on the console
rather than silent.

## Status

Discovery and transport-register access only. Virtqueue setup and block
read/write require the driver to learn the *physical* address of its DMA
rings, which no current syscall exposes; see the report accompanying this
change for the proposed `frame_physical_address` control operation
(seL4's `Page_GetAddress` is the direct precedent).
