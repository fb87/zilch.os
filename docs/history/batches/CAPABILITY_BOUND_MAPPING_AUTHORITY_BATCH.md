# Capability-bound mapping authority batch

Batch: 0104

This batch binds each stage-1 mapping to the exact frame and address-space capability derivations used to authorize it. Capability mutation and mapping creation are serialized by a bounded authority transaction lock. Deleting an authorizing capability removes mappings owned by that derivation; recursive revoke removes mappings owned by descendants before the descendant capabilities are invalidated.

The implementation remains bounded and uses linear scans. It does not claim scalable mapping indexes, restartable revoke, or completed concurrent race certification.
