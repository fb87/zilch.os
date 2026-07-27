# Module: Flattened Device Tree memory discovery

Parses the boot firmware FDT blob supplied by ARM64 in `x0` and extracts a
bounded physical-memory inventory for early kernel resource discovery.

The parser validates the FDT header and structure/string block bounds, supports
one- and two-cell root address/size tuples, imports `memory` node `reg`
properties, imports the FDT memory reservation map and `/reserved-memory` child
ranges, and reserves the FDT blob itself.

It intentionally avoids policy: the memory manager subtracts the discovered
reservations, aligns resulting regions to the architecture page size, and
publishes allocatable ranges to bootinfo.

Current limits are fixed-capacity metadata, no overlay processing, no CPU or
device discovery, and no malformed-tree fuzz corpus.
