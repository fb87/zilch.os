# Memory objects

Frames and page tables are typed kernel objects. A frame owns a bounded reverse-mapping table. Each mapping records a generation-checked address-space object reference, virtual address, permissions, and mapping generation.

The current production-development implementation supports up to eight mappings per frame. Mapping requires readable permissions, rejects unknown permission bits and writable-executable combinations, and remains a bounded foundation rather than the final scalable mapping database.
