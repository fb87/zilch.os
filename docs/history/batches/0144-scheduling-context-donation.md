# Batch 0144: scheduling-context donation

## Outcome

Synchronous IPC transfers the caller's remaining execution budget and effective
priority to the server. Nested calls propagate the same authority through a
bounded chain. Ordered per-CPU queues now drive IPC and fault deadlines.

## Correctness properties

- A chain may contain at most eight donations.
- Donated ticks are consumed before a server's own budget.
- Unused ticks return to the blocked caller on every reply-authority cleanup
  path; consumed ticks remain charged.
- Effective priority follows the highest upstream caller and returns to base
  priority on unwind.
- Timeout entries carry thread generation and deadline, and arming a thread
  replaces its prior entry.
- Timer expiry removes only due entries and returns when the IPC lifecycle lock
  is temporarily unavailable.

## Evidence

`scheduling_context_donation` verifies a two-hop budget transfer and partial
return. `priority_inheritance` verifies a priority-20 server executes at
priority 240. `donation_chain_bound` rejects depth nine. `timeout_queue_order`
verifies deadline publication and expiry, while the existing IPC lifecycle and
pager timeout suites exercise cleanup integration.
