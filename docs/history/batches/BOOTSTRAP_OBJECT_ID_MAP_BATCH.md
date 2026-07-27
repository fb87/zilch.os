# Bootstrap object ID allocation map

Batch: 0110

## Problem

The root memory resource introduced in 0109 registered fixed object ID 48, which
was already occupied by the bootstrap root notification. The second registration
returned `busy`, aborting user-object initialization before the memory inventory
log was emitted.

## Resolution

All fixed object-table IDs are now defined in one `object::bootstrap_id`
namespace. The root memory resource uses ID 82, after the bootstrap VM/vCPU IDs
and below the dynamic-object range beginning at 96.

Compile-time assertions verify that every fixed range is non-overlapping and
remains below the dynamic allocation boundary.

## Runtime expectation

Boot should proceed past SMP bring-up and print the physical-memory inventory,
then complete the existing certification suite including
`memory_resource_delegation`.
