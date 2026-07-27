# Pager failure protocol

Status: production kernel mechanism; userspace restart policy remains open

## Fault ownership

A thread owns at most one pending fault record. Fault delivery records the
thread generation, syndrome, address, and instruction pointer before publishing
`blocked_fault`. A second delivery while that record remains pending is rejected
and the exception path contains the thread; it cannot replace the first
request's reply authority.

Every delivered fault receives a kernel safety deadline. The deadline remains
active while the request is queued and after a pager accepts reply authority.
This is a containment ceiling, not userspace paging policy.

## Reply and mapping transaction

Frame-backed fault reply holds the IPC lifecycle lock across state validation,
mapping, disposition update, and the `blocked_fault` to `ready` transition. The
mapping database lock serializes page-table and reverse-mapping publication.

If another resolver already installed the same address, permissions, and
address-space generation, the later resolver treats the mapping as an
idempotent completion. Only one reverse-mapping record remains. A conflicting
mapping or permission request still fails.

## Failure behavior

- Pager exit with live fault reply authority clears the deadline and reply
  relationship, marks the disposition `terminate`, and terminates the caller.
- A queued or accepted request whose deadline expires is removed from its
  endpoint queue where applicable, any matching reply authority is revoked,
  and the caller is terminated.
- Successful frame-backed or disposition-only resume clears the deadline,
  endpoint selector, and pending fault record.

These transitions are serialized by the IPC lifecycle lock. Timer expiry uses
the existing non-blocking lock attempt and retries on a later tick rather than
spinning in interrupt context.

## Remaining userspace policy

The kernel intentionally does not choose a replacement pager, restart a failed
service, or replay requests. A userspace supervisor must define pager restart,
address-space reassignment, and crash-loop policy before MEM-026 can complete.
A controlled simultaneous multi-CPU same-page fault injection remains required
to complete MEM-025; current certification proves the duplicate resolver
linearization and single-record invariant.
