# native personality

The programming model a zilch userspace process is born into: what it
already holds when its first instruction runs, and the idioms every server
would otherwise restate locally.

Header: `include/sys/native.hh`, on the userspace include path (see
`src/user/user.mk`'s `USER_CPPFLAGS`), so servers reach it as
`#include <sys/native.hh>`.

## Why it exists

Six servers had independently written `service_endpoint = 11U`, five had
written `root_notification = 14U`, four had written
`failure_badge = 1U << 15U`, and three drivers each had their own copy of
the bounded-retry loop with its own attempt constant. Most of those are a
*kernel* contract, not a per-server choice, so a change to the contract had
six places to miss.

## What a process is born holding

Installed by the kernel into every child's cspace by `create_user_bundle()`
(`src/kernel/include/sys/kernel/thread/scheduler.hh`):

| Slot | Capability                                             |
|------|--------------------------------------------------------|
| 1    | own task                                               |
| 2    | own thread                                             |
| 3    | own address space                                      |
| 4    | own scheduling context                                 |
| 10   | fault endpoint, minted from root's slot 10, per-child badged |
| 14   | root's readiness notification (write only)              |
| 15   | memory resource delegated with the task's page quota    |

Slot **11** — a process's own service endpoint — is deliberately listed
apart: it is *not* a kernel contract. Root mints it after each
`process_create` (`root_graph.hh`), so it is a userspace convention that
can be changed by changing root. Blurring that distinction is exactly what
six copy-pasted constants did.

## Idioms

- `retry(attempt)` — bounded retry for a capability root mints only *after*
  `process_create` returns. A child can genuinely start and reach its setup
  code before root's follow-up mint executes, so a one-shot attempt races.
- `signal_ready(badge)` / `signal_failure()` — the readiness protocol root's
  `supervise()` loop waits on.
- `ok(raw)` / `status(raw)` — the raw `word_t` a syscall returns is a negated
  `error_t`; these stop every call site from re-deriving the cast.
- `text::write/hex/line` — diagnostics through a capability to
  serial-driver's service endpoint. A userspace process has no printk, and
  this IPC call *is* the console path, so bring-up would otherwise be silent.

## Adoption

Every userspace server and driver uses it: `drivers/serial`,
`drivers/virtio`, `servers/console`, `servers/memory`,
`servers/control_plane`, `servers/domain`, and the control-plane
certification harness. No raw copy of these constants remains outside this
header.

Servers keep their own local names where they had them (`notification`,
`endpoint`, `resource`) and point those at the personality's constants,
rather than renaming call sites. That keeps the diff to the declarations and
leaves each server's own vocabulary intact.

`init`/`root_graph.hh` deliberately does **not** use these: root wires the
graph *from the outside*, minting into other tasks' cspaces, so its
selectors are about where a capability is being placed, not what root itself
holds. Those are genuinely root's own concern.

The migration was verified by comparing built artifacts rather than by
re-running the timing-sensitive certification suite: all seven user binaries
are bit-identical before and after, which proves the change is
behaviour-preserving in a way a load-dependent latency test cannot.
