# Batch 0136: native ABI v1 freeze

This batch completes PRD-010, PRD-011, PRD-012, and DOC-014.

The native `sys::abi::v1` interface is frozen as ABI 1.0.0. Certification-only fuzz identifiers moved into `sys::test_abi::v1`, leaving the product syscall header free of test endpoints and discriminators.

Host ABI checks now cover every public structure's size, alignment, offsets, standard-layout and trivial-copy properties; every public enum's underlying width; the 64-bit syscall word; and selected published numeric values. Every public header is also compiled independently.

Compatibility and release documents define additive minor changes, required major-version changes, minimum deprecation lifetime, identifier non-reuse, diagnostic-format versioning, and release gates.
