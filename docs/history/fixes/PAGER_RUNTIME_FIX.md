# Pager Runtime Fix

Patch 0068 corrects the certification pager fault address to remain inside the
currently supported ARM64 root-only user VA window. The previous test used
0x30000000 while the backend accepts mappings in the user_code L2 region below
user_stack_base. The allocator summary is also emitted only after physical
memory initialization succeeds.

Runtime evidence required:

- `memory: base=... pages>0 free>0`
- `[TEST] name=pager_fault_reply result=PASS`
- `[TEST] name=dynamic_memory_objects result=PASS`
- final kernel acceptance PASS
