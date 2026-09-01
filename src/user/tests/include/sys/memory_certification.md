# Memory certification cases

Six memory-subsystem certification cases used by the self-test harness:
resource delegation, extent retype, extent metadata, pressure rollback,
resource lifecycle, and mapping database.

They live here rather than in the init executable because that file had
grown past the 1200-line source boundary. These particular cases were the
ones that could move without changing behavior: each drives the memory
control operations directly and needs none of the harness's shared
scaffolding -- no service-process creation or teardown, no readiness-badge
waiting, no access to the acceptance ledger. That is what distinguishes
them from the process-lifecycle and IPC-race cases still in the executable,
which are bound to that scaffolding, and from the control-plane cases,
which were extracted earlier but only by passing the scaffolding in as
callbacks.

Each case returns pass or fail and records nothing itself. The executable
still owns the ledger, so test identity, numbering, and ordering remain in
one place, and moving a case here does not change what the acceptance
report says about it.

Two cases additionally drive test-only control operations to inject extent
allocation failures and to snapshot memory invariants. Those belong to the
certification ABI, not the production one, which is why this header is only
reachable from the self-test include path.
