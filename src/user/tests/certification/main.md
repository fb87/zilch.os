# Certification harness executable

The kernel's legacy ledger-based self-test suite, built as `init.elf` in
place of `src/user/init/main.cc`'s production supervisor entry point when
`CONFIG_SELFTEST=1` (see `src/user/user.mk`'s conditional `USER_init_SOURCE`).
Runs capability, IPC, memory, scheduling, and hypervisor lifecycle checks
directly against the syscall ABI, records each into an acceptance ledger via
`sys::certification::control()`, and reports the aggregate `[ACCEPTANCE]`
result. Structurally independent of `root_graph.hh`'s service-graph
supervision -- this harness never launches the production driver/console
graph, which is why `tools/verification/smoke.sh` exists to cover that path
separately.
