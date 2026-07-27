# Native `sys` ABI compatibility policy

The current native ABI is **1.0.0** and is frozen at the register, numeric-value, size, alignment, and field-offset levels. It is identified by the `sys::abi::v1` namespace and the constants in `version.hh`.

- Patch changes may clarify documentation and fix implementation defects without changing layouts or operation values.
- Minor changes may add operations, enum values, or reserved fields while preserving existing values and layouts.
- Major changes may alter layouts, calling conventions, or semantics and require explicit negotiation.
- Numeric syscall and operation values are never silently reused after publication.
- Existing structure sizes, alignments, field offsets, and enum underlying types do not change within ABI major version 1.
- Reserved fields must be zero on send and ignored on receive until assigned meaning.
- Unsupported operations return `unsupported`; malformed version/layout inputs return `invalid_argument`.
- Certification-only operations are outside `sys::abi` and have no compatibility guarantee.
- A deprecated operation remains implemented for at least one released minor version. Its replacement and removal version must be recorded in the changelog before deprecation begins.
- Removing an operation, changing its semantics incompatibly, or changing the register calling convention requires ABI v2.

The native syscall convention uses 64-bit words: syscall number and arguments are passed through the architecture syscall frame, the primary result is returned in word 0, and operations with a secondary result use word 1. IPC uses eight input words and returns status, sender, and four message words.

`make abi-check` compiles every public structure on the host and verifies sizes, alignments, offsets, standard-layout/trivial-copy properties, enum widths, selected frozen numeric values, and the 64-bit syscall word contract. `make abi-headers-check` independently compiles every public header.
