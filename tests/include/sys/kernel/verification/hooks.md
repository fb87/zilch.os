# bootstrap acceptance runtime acceptance

The runtime acceptance module emits machine-readable `[TEST]` and
`[ACCEPTANCE]` records. A final PASS is emitted once boot-time authority,
memory and notification checks have passed, the expected fault IPC has been
observed, every online CPU advances, at least 1,048,576 deterministic fuzz
operations complete, and the global failure count remains zero.
