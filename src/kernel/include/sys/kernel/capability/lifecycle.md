# Capability lifecycle

The production capability core records parent-child derivation for every copied or minted capability. Rights may only be attenuated. Mint may attach a badge. Move transfers an existing derivation atomically, delete removes one slot, and revoke removes all descendants of a selected derivation across registered CSpaces while preserving the ancestor.

Derivation depth, registered CSpaces, and total derivations are bounded explicitly. Exhaustion returns an error and never overwrites a live capability. Runtime concurrent revoke-versus-IPC verification remains a release gate.
