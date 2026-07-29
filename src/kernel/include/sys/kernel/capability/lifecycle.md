# Capability lifecycle

The production capability core records parent-child derivation for every copied or minted capability. Rights may only be attenuated. Mint may attach a badge. Move transfers an existing derivation atomically, delete removes one slot, and revoke removes all descendants of a selected derivation across registered CSpaces while preserving the ancestor.

Derivation identifiers contain a record index and a nonzero generation.
Inactive records are not reused while an active child still names the exact
old identifier, so deleting an intermediate capability cannot turn its
descendants into an ABA link to an unrelated tree. Generation wrap retires
the bounded record instead of returning to an old identifier.

Derivation depth, registered CSpaces, and total derivations are bounded
explicitly. Exhaustion returns an error and never overwrites a live
capability. The 1.0 production contract uses a maximum depth of 64, 32
registered CSpaces, 256 slots per CSpace, and 4,095 usable derivation records.
Revoke is one bounded, non-restartable authority transaction over those fixed
capacities.

Final certification validates every registered CSpace and active derivation:
registry ownership, guard and allocation bounds, occupancy coherence,
generation encoding, unique live slot ownership, exact parent identity, and
live object resolution must all hold.
