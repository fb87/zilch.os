# SMMU discovery

Discovery and identification of an ARM SMMUv3, and nothing else. Translation
is deliberately not enabled, and this document exists partly to record why, so
the absence is not mistaken for unfinished work.

The device tree is scanned for an `arm,smmu-v3` compatible node during boot
inventory. A node's `reg` base and size are captured provisionally as the
walker encounters them and committed at `end_node`, because `compatible` and
`reg` can appear in either order within a node and an earlier version that
consumed `reg` only after matching `compatible` missed a present SMMU entirely.

When a node is found, IDR0 and IDR1 are read to report whether stage-1 (S1P,
bit 0) and stage-2 (S2P, bit 1) translation are supported and how many stream
identifier bits the implementation provides (SIDSIZE, IDR1 bits 5:0). The
result is reported once at boot: either the identified capabilities or
`smmu: absent (no arm,smmu-v3 node)`.

## Why translation stays off

Nothing this kernel drives sits behind the SMMU on this platform. QEMU's
`virt` machine attaches its SMMUv3 to the PCIe root complex, and the
`iommu-map` property appears only on the PCIe node; the virtio-mmio transports
this kernel actually uses for its block device are not behind it. Programming
stream table entries would therefore translate for no device, while adding a
failure mode -- a mistake in the stream table faults DMA that currently works.

Enabling it meaningfully requires a PCIe root-complex driver first, so that
there is a device whose stream identifiers mean something. That is what keeps
DEV-007 through DEV-018 open in the readiness checklist: they are blocked on a
missing driver rather than on SMMU programming itself.

## Exercising the present path

The default QEMU profile has no SMMU node, so the absent path is what boots
normally. `ZILCH_SMMU=1` adds `iommu=smmuv3` to both the device-tree dump and
the run invocation, which is how the discovery and identification path is
exercised end to end.
