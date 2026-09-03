# Module: native

## Purpose

The programming model a zilch userspace process is born into: what it
already holds when its first instruction runs, and the idioms every server
would otherwise restate locally. See
`src/user/personalities/native/README.md` for the full rationale (six
servers had independently written the same well-known slot constants) and
the adoption list.

## Responsibilities

- Name the capability slots `create_user_bundle()` installs into every
  child's cspace (own task/thread/space/scheduling-context, the per-child
  badged fault endpoint, root's readiness notification, the delegated
  memory resource), and separately name `service_endpoint` -- the one slot
  in that list that is a root-side userspace convention, not a kernel
  contract, and says so.
- `retry()`: the bounded-retry idiom for a capability root mints only
  *after* `process_create` returns, so a one-shot attempt would race.
- `signal_ready()`/`signal_failure()`: the readiness protocol
  `root_graph.hh`'s `supervise()` waits on.
- `ok()`/`status()`: the raw `word_t` <-> `error_t` conversion every
  syscall call site otherwise re-derives.
- `text::write/hex/line`: diagnostics through a capability to
  serial-driver's service endpoint -- a userspace process has no printk,
  and this IPC call is the only console path available to it.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- `service_endpoint` is documented as non-kernel precisely so a future
  reader does not assume changing it requires a kernel change.

## Verification

Adopted by every userspace server and driver; see the README's adoption
list. Behaviour-preservation of the migration itself was verified by
comparing built artifacts (every user binary bit-identical before and
after), not by a runtime test of this header specifically.
