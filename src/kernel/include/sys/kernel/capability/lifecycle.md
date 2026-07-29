# Capability lifecycle

The production capability core records parent-child derivation for every copied or minted capability. Rights may only be attenuated. Mint may attach a badge. Move transfers an existing derivation atomically, delete removes one slot, and revoke removes all descendants of a selected derivation across registered CSpaces while preserving the ancestor.

Derivation identifiers contain a record index and a nonzero generation.
Inactive records are not reused while an active child still names the exact
old identifier, so deleting an intermediate capability cannot turn its
descendants into an ABA link to an unrelated tree. Generation wrap retires
the bounded record instead of returning to an old identifier.

Derivation depth, registered CSpaces, and total derivations are bounded
explicitly. Exhaustion returns an error and never overwrites a live
capability. Scalable restartable revoke remains a release gate.
