# serial

Role `0x107`. The exclusive owner of the physical PL011 UART, split out of
the console service so that hardware ownership, TX service, and RX service
are no longer one loop in one process.

Root creates both root-gated capabilities this driver needs -- the PL011
device frame (`device_frame_create`) and its IRQ (`interrupt_create`) --
and mints them in before this process runs, mirroring how it already mints
device frames into the domain manager. Everything after that is
unprivileged: this driver maps the frame, configures the device, binds the
IRQ to its own notification, and acknowledges it.

RX is interrupt-driven. `IMSC.RXIM` is unmasked at the device, and the
hardware FIFO is drained into a 64-byte ring buffer only when the bound
notification actually signals, rather than re-reading `FR.RXFE` on every
wakeup. The ring buffer is load-bearing: one interrupt can carry several
queued bytes, and a single-byte holding cell drops the rest.

The IRQ number is GIC INTID 33 (SPI 1, level-triggered), read off this
platform's real device tree (`pl011@9000000` declares
`interrupts = <0 1 4>`) rather than assumed.

Serves `sys::abi::v1::serial_operation` (`write`, `write_byte`,
`read_byte`) over capability-protected IPC to its one client, the console
server. That enum is deliberately separate from `control_plane_operation`,
which carries role-dispatch semantics this driver does not participate in
-- the same reasoning behind `memory_server_operation`.

Known limitations, both deliberate:

- The main loop still wakes on a bounded `ipc_receive` timeout instead of
  sleeping on the interrupt, because this kernel has no blocking-wait
  syscall for notifications (`notification_poll` is an instant
  read-and-clear). The interrupt still earns its place: drain work happens
  only on real signals, with correct `IMSC`/ack lifecycle and multi-byte
  capture.
- Not covered by restart-on-fault (no `service_policy` entry), the same
  boundary already drawn for the memory server.
- TX remains polled. The vPL011 layer already batches guest output, so RX
  was where blind polling actually cost something.
