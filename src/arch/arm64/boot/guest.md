# ARM64 Hypervisor Profile 0.1 guest payload

This preprocessed assembly module is the bounded first guest image used by the
Hypervisor Profile 0.1 runtime gate. It executes at guest EL1 with stage-1 MMU
disabled and stage-2 translation enabled by Zilch EL2.

The payload emits `[GUEST] boot`, queries the architectural counter through the
versioned guest hypercall ABI, emits `[GUEST] time`, and requests clean shutdown.
It contains no host physical addresses and can access only the 64 KiB guest IPA
window installed by the VM capability owner.

Unexpected exits are diagnosed by the EL2 backend rather than by this payload.
