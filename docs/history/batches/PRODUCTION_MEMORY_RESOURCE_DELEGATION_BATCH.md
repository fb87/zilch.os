# Batch 0109: Userspace Memory-Resource Delegation

This batch introduces bounded memory-resource capability objects. Root owns the managed-page resource and process construction delegates a child quota to each PL3 task. The memory server allocates pager frames through its resource selector. The mechanism is an intermediate accounting/delegation model, not physical extent retyping.
