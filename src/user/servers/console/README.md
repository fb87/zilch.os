# Console service

Role `0x202`, ordered after process and device policy, running as an
unprivileged PL3 service.

It no longer owns the UART. Hardware ownership moved to the serial driver
(`src/user/drivers/serial`, role `0x107`); this server is now a pure IPC
relay that forwards character I/O to that driver over
`sys::abi::v1::serial_operation`.

It runs as two independent threads rather than one loop:

- The **main thread** (role `0x202`) serves `write`, `write_byte`,
  `health`, `describe`, and `stop` on its service endpoint.
- A **stdin thread** serves `read_byte` on a separate, dedicated endpoint.
  The server spawns it itself with `thread_create` under reserved role
  `0x108`, bound to this same binary -- the mechanism root already uses for
  its own supervision thread. `main()` dispatches on the role argument to
  pick the thread's entry path.

Neither thread shares state with the other; each only forwards its own
operation type and replies, so no cross-thread coordination is needed.

The stdin thread is spawned only after its endpoint capability resolves.
Root can mint that capability only after `process_create` returns it a
task capability, so this process genuinely can start first; spawning
early left the new thread failing capability resolution immediately and
spinning hot, which starved its CPU and hung boot outright.

`sys::console` (`src/user/include/sys/console_client.hh`) is unchanged in
shape -- `write`, `write_byte`, `read_byte` -- but `read_byte` must be
called against this server's stdin endpoint, not its write endpoint. Root
mints both into clients that need RX (see the domain manager).
