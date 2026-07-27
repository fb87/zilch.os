# Batch 0125: authority-safe capability mutation API

Capability correctness previously depended on each caller remembering to take
the global authority lock. IPC transfer and control operations did so, but the
general mutation API still permitted an unprotected call.

Public install, derive, copy, mint, move, remove, and delete operations now
acquire the authority lock themselves. Explicitly named `_locked` primitives
serve code already executing inside a larger authority transaction:

- control-path copy, mint, move, and delete;
- IPC capability transfer.

This structure makes omitted serialization the difficult path while retaining
multi-step transaction atomicity and avoiding recursive spin-lock acquisition.

Evidence:

- four-CPU certification acceptance passes with zero failures;
- capability lifecycle and transfer/revoke race tests pass;
- downstream pager, memory, SMP fuzz, and object destroy/reuse tests pass.
