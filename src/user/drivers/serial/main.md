# Serial driver executable

The independently linked PL3 serial driver owns the physical PL011 UART for
role `0x107`. It is not a control-plane role and does not participate in
that role range's dispatch semantics, so it is launched by a direct
`process_create` outside root's fixed five-slot control-plane loop, with its
own selector range, service endpoint, and readiness badge -- the same wiring
the memory server already uses.

Both privileged capabilities it depends on are created by root and minted in
before this process runs: the PL011 device frame (`device_frame_create`) and
its interrupt (`interrupt_create`). Everything the driver itself does is
unprivileged -- mapping the frame, configuring the device, binding the
interrupt to its own notification, and acknowledging it. Both mints race the
driver's own startup, so the frame mapping retries under a bounded attempt
count rather than assuming root won.

Receive is interrupt-driven. The driver unmasks `IMSC.RXIM` at the device and
drains the hardware FIFO into a bounded ring buffer only when the bound
notification signals, instead of re-reading the receive-empty flag on every
wakeup. The ring buffer is required rather than incidental: a single
interrupt can represent several queued bytes, and a one-byte holding cell
discards the remainder. The interrupt is level-triggered, so the FIFO is
emptied before acknowledging.

The interrupt number is a property of the platform's device tree, not a
constant chosen here.

Transmit remains polled. The virtual PL011 layer in the domain manager
already batches guest output, so receive was the path where unconditional
polling had a measurable cost.

The driver serves a private operation set -- string write, single-byte write,
and single-byte read -- over capability-protected IPC to one client, the
console service. That operation enum is deliberately separate from the
control-plane operation enum, matching how the memory server defines its own.

Two limitations are deliberate. The main loop wakes on a bounded receive
timeout rather than sleeping on the interrupt, because the kernel exposes no
blocking wait for notifications; the interrupt still confines drain work to
real signals and provides correct mask/acknowledge lifecycle. The driver is
also outside restart-on-fault admission, the same boundary drawn for the
memory server.
