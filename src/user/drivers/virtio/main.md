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
| 21   | interrupt (INTID 79, transport 31)            |
| 22   | notification bound to that interrupt          |
| 23   | serial-driver service endpoint (diagnostics)  |
| 24   | own DMA frame (created by this driver)        |
| 25   | shared payload frame (created by root)        |

Bring-up diagnostics are written as text through slot 23 — the same endpoint
console-server forwards to — so a probe scan is observable on the console
rather than silent.

## DMA

Virtqueues are DMA structures: the device is programmed with the *physical*
addresses of the descriptor table, available ring, and used ring. Userspace
had no way to learn a frame's physical address, so this driver motivated
`abi::v1::control_operation::frame_physical_address` (seL4's
`seL4_ARM_Page_GetAddress` is the direct precedent). It is gated on the
frame's **control** right, which a task creating its own frame holds
(`create_frame` installs `read|write|grant|control`) but a merely-delegated
read/write capability does not — so a frame handed to a client discloses
nothing new.

The driver creates its own DMA page. Note that `frame_create` already calls
`assign_frame` internally, so the frame comes back allocated and physically
backed; calling `frame_allocate` on top of it fails with `busy`.

`memory::valid_attributes()` requires a normal RAM frame to be mapped
`normal` + `inner_shareable`, so the rings are ordinary cacheable memory.
That is safe here because the device tree marks these transports
`dma-coherent` (verified), making the device coherent with CPU caches — so
ordering, not cache maintenance, is the only requirement. A `dsb sy` sits
between publishing a ring entry and publishing the index that exposes it,
and after observing an index the device wrote.

Layout inside the single 4 KiB page (queue size 8):

| Offset  | Structure                    |
|---------|------------------------------|
| `0x000` | descriptor table (8 × 16 B)  |
| `0x080` | available ring               |
| `0x100` | used ring                    |
| `0x200` | request header (16 B)        |
| `0x210` | status byte                  |
| `0x400` | data buffer (one sector)     |

## Status

Discovery, modern-transport initialisation, and single-sector read/write
over a split virtqueue. A boot self-check writes a pattern to the last
sector, clears the buffer, reads it back, and reports PASS/FAIL, so the DMA
path is proven end to end at every boot rather than merely assumed.

## Interrupts

Root creates one interrupt capability, for INTID 79 (transport 31), and the
driver binds it to a notification before setting `DRIVER_OK` — so no
completion can be raised on an unbound line. The driver checks that the
transport it discovered actually corresponds to that INTID and falls back to
polling, saying so, rather than waiting on a line that would never fire.

Whether a *particular* request's interrupt is observed is inherently racy:
QEMU services the doorbell write inline, so the used ring has usually already
moved by the time the first poll runs. The used ring, not the interrupt, is
therefore the authoritative completion signal; the interrupt's roles are
keeping the edge-triggered line acknowledged (the kernel masks on delivery
and `interrupt_ack` re-arms it) and providing the hook an asynchronous
completion path would use. The boot self-check reports `irq=live` by checking
both the running signal count and a final drain, which is stable regardless
of which request wins the race.

The request path still spins, because this codebase has no blocking wait for
a notification — the same constraint `serial-driver`'s loop and
`root_graph.hh`'s `drain_fault_reports()` work around.

## Payload buffer

Transfer payload lives in a **separate frame** from the ring page. That
separation is the point: the ring page holds the descriptor table and the
available/used rings, so a client granted access to it could corrupt the
queue the driver is running. The payload frame carries only sector bytes.

Root creates it and mints it to the driver with `control` as well as
read/write, because the driver must call `frame_physical_address` on it to
aim a descriptor at it. A client would get read/write only — enough to fill
or drain a sector, not enough to ask where it physically lives.

The boot self-check writes a distinct offset-derived value into every 8-byte
slot of a sector, clears the buffer, reads it back, and reports how many
bytes verified (`bytes=0x200` for a full sector). Deriving each value from
its offset means a short or misaligned transfer cannot coincidentally match,
which a single-word check would not have caught.

## Verification from a client

The boot self-check above runs entirely inside the driver, so it proves the
virtqueue but says nothing about whether the IPC service or the shared-frame
wiring work for anyone else. Root therefore also exercises the service as a
genuine client (`root_graph.hh`'s `verify_block_service()`): it fills the
payload frame, calls `write`, **clears the frame**, calls `read`, and
verifies all 512 bytes. Clearing in between is what makes it a round trip
rather than a check that the buffer still holds what was just put there.

Root is a real client rather than a synthetic one — it created the payload
frame and the service endpoint, so it already holds both and no capability
is minted specially for the check. It does hold the frame with full rights
(having created it), so this exercises the cross-task path but not
specifically the reduced read/write-only rights a delegated client would get.

A machine with no disk reports `block-service absent` and boots normally;
only a device that is present but does not work fails the boot. Verified
both ways (`BLOCK_IMAGE=-` for the absent case).

## Remaining

The `read` reply still echoes the leading 24 bytes inline, for a client that
has the endpoint but not the payload frame.

Transfers are one sector per request; multi-sector would chain more
descriptors, which the queue of 8 has room for. A second concurrent client
would need its own payload frame, since there is currently exactly one.
