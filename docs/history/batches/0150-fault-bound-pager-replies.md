# Batch 0150 — Fault-bound pager replies

- Require a pager mapping to cover the page recorded in the pending fault.
- Require writable permission for data-abort writes, readable permission for
  data-abort reads, and executable permission for instruction aborts.
- Reject unsupported fault kinds and malformed mappings before address-space
  mutation.
- Preserve blocked state, pending disposition, and one-shot reply authority
  after a rejected reply so userspace can retry or terminate.
- Certify wrong-page and read-only/write-fault rejection in both the kernel
  fixture and the real PL3 userspace pager before a successful corrected retry.
