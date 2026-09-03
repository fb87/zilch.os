# Module: main

## Purpose

Production `init.elf` entry point: hands control to `root_graph.hh`'s
service-graph supervision (`supervise()`), or to the supervision thread
entry point when launched with `root_supervisor_role`. Built in place of
`src/user/tests/certification/main.cc` whenever `CONFIG_SELFTEST=0` (see
`src/user/user.mk`'s conditional `USER_init_SOURCE`).

## Verification

Verification is tracked by colocated tests or system-level tests
(`tools/verification/smoke.sh`).
