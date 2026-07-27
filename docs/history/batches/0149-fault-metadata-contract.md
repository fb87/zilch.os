# Batch 0149 — Frozen fault IPC metadata contract

- Define the ABI v1 `fault_message` as four 64-bit words: kind, architecture
  syndrome, fault address, and instruction pointer.
- Deliver the recorded syndrome instead of an unused disposition placeholder.
- Keep pager disposition exclusively in the reply path.
- Freeze structure size, alignment, field offsets, enum widths, and selected
  numeric values in the ABI gate.
- Validate all fields from two real PL3 data aborts and a real PL3 undefined
  instruction before resuming or terminating the clients.
