# Capability and IPC semantics

Status: implemented ARM64 kernel contract

This describes the rules an independent implementation would have to reproduce
to be compatible. It complements rather than repeats three existing documents:
`CSPACE_DESIGN.md` for selector geometry, capacity and locking;
`IPC_BADGE_PROTOCOL.md` for badge capture and identity; `FAULT_IPC_PROTOCOL.md`
for fault delivery.

## Authority

All authority is a capability in a CSpace slot, named by a selector. There is
no ambient authority and no namespace: a thread can act on an object exactly
when a capability to it sits in a slot it can name.

### Rights

Six independent bits, checked per operation:

| Right | Bit | Gates |
| --- | --- | --- |
| `read` | 0 | Receiving on an endpoint; reading a frame; observing object state |
| `write` | 1 | Calling/sending on an endpoint; writing a frame; signalling a notification |
| `execute` | 2 | Entering a vCPU (`vcpu_run`). Not a memory permission — mapping permissions are carried by the mapping, not by this bit |
| `grant` | 3 | Delegating the capability onward, rights-attenuated |
| `control` | 4 | Lifecycle operations on the object — binding a notification to an interrupt, destroying a VM or vCPU, reaping, reading a frame's physical address |
| `manage` | 5 | Revoking a capability's descendants, as an alternative to `grant` |

Attenuation is one-directional: a derived capability may drop rights, never add
them. `grant` is what permits derivation at all, so a capability without it is
a leaf and cannot be propagated.

`grant` and `manage` are alternatives for descendant revocation: holding either
authorizes it. That lets a manager revoke a subtree it handed out without also
being able to hand out further copies itself, and conversely lets a delegator
clean up what it granted.

The `control`/`read`+`write` split is deliberate and load-bearing in at least
one place worth calling out: `frame_physical_address` requires `control`, which
a task creating its own frame holds but a task merely handed a read/write
capability does not. A frame delegated to a client therefore discloses nothing
about where it lives in physical memory.

### Generation checking

Every capability names its object through a generation-tagged reference.
Resolution compares the reference's generation against the object table slot's
and the object header's, and fails if either differs. A capability naming a
destroyed object therefore fails closed rather than addressing whatever later
reused the slot; this is what makes bounded object pools safe to recycle.

Revocation severs authority, not only the name. Revoking a capability runs a
type-specific teardown hook — see `INTERRUPT_LIFECYCLE.md` for what that means
for a device line and its MMIO frame.

## Syscall register convention (ARM64)

`ABI_COMPATIBILITY.md` freezes the register calling convention at v1 but does
not state it. It is:

- Entry is `SVC` from EL0, identified by vector 8 with ESR EC `0x15`.
- `x8` carries the syscall number.
- `x0`–`x7` carry arguments, by index. An index at or beyond 8 reads as zero.
- `x0` carries the primary result on return.
- `x1`–`x7` carry additional out-parameters for calls that report more than
  one value, written by index.

For IPC specifically: `x0` is the endpoint selector, `x1` the operation, `x2`–
`x5` the four message words, `x6` the capability-transfer descriptor, and `x7`
the timeout descriptor. On return `x0` is the status, `x1` the sender badge
(or, on a notification wake, the consumed badge set), and `x2`–`x5` the
received message words.

## IPC

Five operations (`abi/sys/v1/syscall_numbers.hh`):

| Operation | Value |
| --- | --- |
| `call` | 0 |
| `receive` | 1 |
| `reply_receive` | 2 |
| `cancel` | 3 |
| `reply` | 4 |

A message is four registers. Anything larger travels through a shared frame.

### Thread states

`inactive`, `ready`, `running`, `blocked_send`, `blocked_receive`,
`blocked_reply`, `blocked_fault`, `suspended`, `faulted`, `terminated`.

Only the four `blocked_*` states participate in IPC. A thread in any of them is
eligible for timeout expiry; a thread in any other state is not, and an expiry
that finds one simply clears the armed flag.

### Receive

`receive` on an endpoint resolves it with `read` right, then, in order:

1. If a sender is queued, rendezvous immediately: the message words are copied
   into the receiver's frame, a reply capability is installed in the receiver,
   and the sender moves to `blocked_reply`.
2. Otherwise, if the endpoint already has a registered receiver, return `busy`.
   One receiver per endpoint; a server that wants concurrency uses more
   endpoints and more threads.
3. Otherwise, if the calling thread has a bound notification with pending
   badges, consume them and return `notification_signal` (see below) without
   blocking.
4. Otherwise publish `blocked_receive`, register as the endpoint's receiver,
   and block.

Steps 3 and 4 both happen while holding the IPC lifecycle lock, and that is
what makes a bound notification's wake safe. A signal landing between "decided
to block" and "recorded as blocked" would otherwise OR its badge into the
pending set and find nobody blocked to wake — a silent missed wakeup.

### Reply capabilities

A rendezvous installs a **one-shot** reply capability in the serving thread,
recording the caller's id, the caller's object generation, and a globally
unique non-zero nonce. It is not addressable from userspace: a server replies
to "whoever I am serving", and cannot name or forge another thread's caller.

Two consequences an implementation must get right:

- **Installation is unconditional and overwrites.** A server thread holds
  exactly one reply slot. If it defers a reply — returns to `receive` intending
  to answer later — the next call that reaches *that same thread* destroys the
  stashed reply, and the original caller blocks forever. A server that defers
  replies must therefore receive on an endpoint no other traffic reaches,
  served by a thread that handles nothing else. The serial driver's split of RX
  onto its own thread and endpoint exists for exactly this reason.
- **Installation donates scheduling.** The caller's scheduling context is
  donated to the server for the duration, so a high-priority caller does not
  wait behind the server's own lower priority. Donation is released when the
  reply completes.

`reply` does not traverse an endpoint capability. It therefore carries no badge
and the caller observes a zero sender word.

### Capability transfer

A call or reply may carry at most four capabilities. Each element names a
source selector in the sender and a destination selector chosen by the
*receiver*. Any duplicate destination, occupied destination, or mint failure
rolls the whole batch back: transfer is all-or-nothing, so a receiver never has
to reason about a partially applied set.

### Timeouts

The timeout descriptor sets bit 63 (`ipc_timeout_valid`); the remaining bits
are a tick count at the platform's 100 Hz scheduler tick. Zero means no
timeout, i.e. block indefinitely.

The deadline is computed against the *calling thread's own CPU* tick counter
and the entry is queued on that CPU's timeout queue, which is also the CPU that
sweeps it. Both ends therefore read the same clock. This matters because
`platform::timer::ticks()` is per-CPU and the counters diverge — see checklist
0158 for a case where mixing them across CPUs broke a different mechanism.

Expiry cancels the thread's endpoint registration and returns `timed_out`.

## Notifications

A notification is a 64-bit set of badge bits, not a counter. `signal` ORs a
badge into the pending set; repeated signals with the same badge collapse. A
consumer therefore learns *that* something happened, never how many times, and
protocols must be edge-tolerant.

`notification_poll` consumes and returns the pending set without blocking.

`notification_bind` associates a notification with the **calling thread only** —
there is no thread-selector parameter, deliberately, so a thread can never bind
a notification to another thread. It requires `write` right, is sticky across
receives, and fails with `busy` if that thread already has a live bind rather
than silently replacing it. `notification_unbind` clears it and is a no-op
success if nothing was bound.

Once bound, a blocked `receive` also wakes on a signal. That wake is
distinguished by a reserved status value rather than by a message convention:

- `error_t::notification_signal` is **1**, a positive value in the same word
  that ordinary results use, where every other outcome is `success` (0) or a
  negative error. Nothing before it produced a positive value there, so no
  existing caller can misread it.
- The consumed badge set is returned in the first result register and the four
  message words are zeroed, so a notification wake cannot be mistaken for a
  message whose contents happen to look like one.

## Error taxonomy

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | `success` | |
| 1 | `notification_signal` | Not a failure — a bound notification woke a blocked receive |
| -1 | `invalid_argument` | Malformed selector, out-of-range parameter, unknown operation |
| -2 | `unsupported` | Operation not implemented for this object type or platform |
| -3 | `no_memory` | A bounded pool is exhausted; fails closed |
| -4 | `denied` | Capability lacks the required right, or the operation is root-gated |
| -5 | `busy` | Contended or already in the requested state — an endpoint that already has a receiver, a notification already bound, an occupied destination slot, or a poll-style operation whose answer is "not yet" |
| -6 | `not_found` | Resolution failed: empty slot, generation mismatch, or nothing to acknowledge |
| -7 | `timed_out` | An IPC timeout expired |

`busy` carries real weight rather than being a generic failure. This kernel has
no blocking wait primitive: `process_wait` and `notification_poll` return
`busy` to mean "ask again". Callers must therefore poll — and because every
poll resolves a capability and takes the global authority lock, the lock is a
ticket lock specifically so a poller cannot starve what it is waiting for. See
`LOCKING_PROTOCOL.md`.
