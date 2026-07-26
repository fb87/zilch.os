# Capability lifecycle

K2 adds bounded CSpace copy, move, delete and reference revocation. Copy requires `grant`; rights may only be reduced. Move is atomic across the source and destination CSpaces. Object destruction revokes matching generation-bound references before object-table reuse.
