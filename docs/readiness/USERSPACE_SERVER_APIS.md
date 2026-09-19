# Userspace server APIs

Status: complete for the QEMU ARM64 1.0 platform profile

Every service in the control-plane graph is an ordinary process reached only
through capability-protected endpoints. There is no ambient namespace and no
name lookup: a client can call a server exactly when root has minted that
server's endpoint capability into a known slot of the client's CSpace before
the client ran. The slot numbers are part of the contract and are fixed in
`src/user/include/sys/root_graph.hh`; a server and its clients agreeing on a
number is what stands in for a name service.

All calls use `ipc_call` with the operation in `message0` and operation
arguments in `message1..message3`. Replies put a status or result in
`message0`. Unknown operations return `invalid_argument` rather than being
ignored, so a protocol mismatch fails loudly at the first call.

## Common control-plane operations

`abi/sys/v1/control_plane.hh`. Every role serves these on its private service
endpoint regardless of what else it does.

| Op | Value | Contract |
| --- | --- | --- |
| `health` | 0 | Replies with the protocol magic and the role's exact identifier. The liveness probe the readiness loop and restart verification both use. |
| `describe` | 1 | Replies with the role's dependency mask, memory quota and restart limit. |
| `stop` | 2 | Replies *first*, then exits and publishes the role's lifecycle badge. Reply-before-exit is required: a role that exited first would leave its caller blocked forever. |
| `inject_fault` | 11 | Faults deliberately so restart-on-fault can be exercised. Honoured only when `CONFIG_FAULT_INJECTION` is set, otherwise treated as unknown. The caller never receives a reply — the role dies before replying — so it **must** use a timeout. |

Operations 3–7 (`launch`, `destroy`, `load`, `run`, `serve`) are meaningful
only to the domain role, and 8–10/12 only to the console role. A role that does
not implement an operation returns `invalid_argument` for it; the operation
space is shared, the implementations are not.

## Console server

`src/user/servers/console`. Owns no hardware. Everything below forwards to
serial-driver over IPC; the console server exists to multiplex many clients
onto the one driver and to be the thing clients hold a capability to.

| Op | Value | Contract |
| --- | --- | --- |
| `write` | 8 | Up to `console_write_max_bytes` (24) packed into `message1..message3`, eight bytes per word, little-endian, NUL-padded. The host-diagnostic path. |
| `write_byte` | 9 | Low byte of `message1`. Used for guest vPL011 forwarding, where byte-at-a-time ordering matters. |
| `read_byte` | 10 | No payload. Replies `message0 = 1` and the byte in `message1` if one was available, `message0 = 0` otherwise. Never blocks. |
| `read_byte_wait` | 12 | Same wire shape, but blocks until a byte exists. |

The split between `read_byte` and `read_byte_wait` is load-bearing in both
directions. The interactive shell's `read()` needs the blocking form, or it
busy-polls the console. domain-manager needs the non-blocking form, because its
VM idle-exit handling depends on being told "nothing available" promptly.

Reads are served by a **second thread** on a second endpoint, independent of the
write-serving loop. That is not throughput work: `read_byte_wait` defers its
reply, a deferred reply lives in the serving thread's single reply slot, and the
kernel overwrites that slot on the next call to reach the same thread. A write
arriving while a read was parked therefore used to strand the reader — and
stdin with it — permanently.

## Serial driver

`src/user/drivers/serial`, `abi/sys/v1/serial.hh`. The only holder of the PL011
device frame and its IRQ.

| Op | Value | Contract |
| --- | --- | --- |
| `write` | 0 | Packed multi-byte write, same packing as the console server's `write`. |
| `write_byte` | 1 | Single byte from `message1`. |
| `read_byte` | 2 | Non-blocking pop from the RX ring. |
| `read_byte_wait` | 3 | Defers its reply until the ring has a byte. |

RX is served on its own thread and its own endpoint for the deferred-reply
reason above, and that thread binds the UART interrupt notification to itself.
Because `thread_create` gives each sibling thread its **own** address space and
shares only the CSpace, the RX thread maps the UART frame separately at the same
scratch address; a mapping made by the main thread is not visible to it.

Bytes reach the ring only from the interrupt path. The drain sequence clears the
PL011 RX interrupt *before* each drain pass and repeats until the FIFO is
genuinely empty; the reverse order loses the interrupt permanently, because the
line asserts on the FIFO crossing its trigger level rather than while it sits
above it.

Known defect: input stops being delivered after a variable number of commands.
See PRODUCTION_READINESS_CHECKLIST entries 0156 and 0159 — it is a timing race
that disappears under instrumentation, and it is not diagnosed.

## Block service

`abi/sys/v1/virtio.hh`. Backed by a virtio-mmio device.

| Op | Value | Request | Reply |
| --- | --- | --- | --- |
| `info` | 0 | none | `m0` = status, `m1` = capacity in 512-byte sectors, `m2` = sector size |
| `probe` | 1 | `m1` = transport index within the granted page | `m0` = status, `m1` = that transport's device id, `m2` = its version |
| `read` | 2 | `m1` = sector index | `m0` = status; the sector lands in the shared frame, and its leading 24 bytes are *also* echoed into `m1..m3` |
| `write` | 3 | `m1` = sector index; caller has already filled the frame | `m0` = status |

`probe` is transport diagnostics, not a presence check.

The echo on `read` is for a client that holds the endpoint but not the payload
frame. `write` deliberately takes nothing from the message words — reading them
would clobber the bytes the caller placed in the frame.

Bulk data moves through a frame the client and server both map, not through
message registers. Programming the device requires physical addresses, which is
why `frame_physical_address` exists as a control operation and why it demands
the frame's `control` right rather than mere read/write: a frame merely
delegated read/write to a client discloses nothing about where it lives.

When no block image is supplied the service reports absent rather than failing
to start, and the VFS server above it then serves only its RAM-backed
filesystem.

## VFS server

`abi/sys/v1/vfs.hh`. A path-addressed filesystem over the block service, plus a
RAM-backed tree rooted at `/tmp` that works with no block device present.

Path and bulk bytes travel in a 4096-byte shared buffer (`vfs_buffer_size`);
message registers carry only lengths, handles and status. Paths are at most
`vfs_max_path` (255) bytes and a client may hold `vfs_max_open_files` (32).

| Op | Value | Request | Reply |
| --- | --- | --- | --- |
| `open` | 0 | `m1` = path length in the buffer from offset 0; `m2` = flags | `m0` = status, `m1` = handle |
| `read` | 1 | `m1` = handle; `m2` = length, clamped to the buffer size | `m0` = status, `m1` = bytes read; bytes land in the buffer |
| `write` | 2 | `m1` = handle; `m2` = length already placed in the buffer | `m0` = status, `m1` = bytes written |
| `close` | 3 | `m1` = handle | `m0` = status |
| `stat` | 4 | `m1` = path length | `m0` = status, `m1` = size, `m2` = `vfs_file_type` |
| `readdir` | 5 | `m1` = directory handle; `m2` = 0-based entry index | `m0` = status (`not_found` past the last entry), `m1` = name length, `m2` = type; name in the buffer |
| `seek` | 6 | `m1` = handle; `m2` = new absolute position | `m0` = status, `m1` = resulting position |

Open flags mirror `unistd.h`'s `O_*` bits, because libc is the only caller.

`readdir` re-walks the directory from the start on every call rather than
holding a server-side cursor. Directories here are earlyfs-scale, so the
repeated walk costs nothing a caller would notice, and it avoids per-client
iteration state that would need invalidating on every mutation.

Handles are server-side state keyed to the calling process, not tied to which
image is running. That is what lets a redirected descriptor survive `execv`:
libc passes the handle number across the image replacement as an internal
environment entry and the new image's runtime reinstalls it before `main` runs,
with no reopen by path.

A client must map the shared VFS frame before its first call. `fork` resets the
child's "already mapped" flag deliberately — the flag is ordinary process
memory the child inherits as true, but says nothing about whether the child's
own page table entry still points at the shared frame rather than a private
copy.

## Coreutils and program loading

`src/user/include/sys/coreutils.hh` is the single table shared by root, which
binds each coreutil's role at boot, and libc's `execv`, which resolves a name
against it. Sharing one table is what keeps the two from drifting.

`execv` resolves only these fixed names. It does not search a path or load bytes
from a file: `process_exec` selects an already-bound role, so a forked child can
exec any coreutil without holding the root-gated right to bind one. Loading an
arbitrary program from disk is unimplemented, and deliberately so rather than by
oversight — see that header for the reasoning.
