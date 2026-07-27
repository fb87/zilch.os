# Native `sys` ABI compatibility policy

The current native ABI is development version **1.0.0**. The version identifies the wire/register layout, not a promise that every operation is frozen for Zilch 1.0.

- Patch changes may clarify documentation and fix implementation defects without changing layouts or operation values.
- Minor changes may add operations, enum values, or reserved fields while preserving existing values and layouts.
- Major changes may alter layouts, calling conventions, or semantics and require explicit negotiation.
- Numeric syscall and operation values are never silently reused after publication.
- Reserved fields must be zero on send and ignored on receive until assigned meaning.
- Unsupported operations return `unsupported`; malformed version/layout inputs return `invalid_argument`.
- Certification-only operations are outside `sys::abi` and have no compatibility guarantee.

`make abi-check` compiles the public structures on the host and verifies sizes, alignments, offsets, enum widths, and the 64-bit syscall word contract.
