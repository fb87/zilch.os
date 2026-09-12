#pragma once

#include <sys/kernel/boot/fdt.hh>
#include <sys/kernel/printk.hh>
#include <sys/types.hh>

/*
 * ARM SMMUv3 discovery (DEV-006).
 *
 * The SMMU is the one device in this system the kernel must own rather than
 * delegate. Every other device is handed to a userspace driver through a
 * capability; an SMMU is what makes that delegation safe in the first place,
 * because it is what stops an assigned device's DMA from reaching memory its
 * owner was never given. A userspace driver holding the SMMU could remove
 * its own containment.
 *
 * This is discovery and identification only, deliberately. What it does NOT
 * do is program a stream table, install translation contexts, or enable
 * translation -- and the reason is a property of the platform rather than a
 * shortage of effort. On QEMU's virt machine the SMMUv3 sits in front of the
 * PCIe root complex only: the device tree gives `iommu-map` to pcie@10000000
 * and no `virtio_mmio@...` node carries an `iommus` property at all.
 * Verified by dumping the DTB of the very machine tools/run/run.sh boots.
 * This kernel's only real device is virtio-mmio, which bypasses the SMMU
 * entirely, so a stream table programmed here would translate for nothing
 * and every assertion about it would be vacuous.
 *
 * Reaching the translation path therefore needs a DMA-capable device behind
 * the SMMU, which on this machine means a PCIe device, which means a PCIe
 * root-complex driver (ECAM enumeration, BAR assignment, MSI through the
 * ITS). That is a subsystem this kernel does not have and which the
 * checklist does not list as a prerequisite. See the checklist's DEV-007 to
 * DEV-018 for how that is recorded.
 */
namespace sys::kernel::smmu
{
    // Register offsets within the SMMUv3 page-0 window, from the ARM SMMUv3
    // architecture specification.
    inline constexpr u64 idr0_offset = 0x000U;
    inline constexpr u64 idr1_offset = 0x004U;
    inline constexpr u64 idr5_offset = 0x014U;

    struct capabilities final {
        bool present{};
        paddr_t base{};
        u32 idr0{};
        u32 idr1{};
        u32 idr5{};
    };

    inline capabilities discovered{};

    [[nodiscard]] inline u32 read32(paddr_t base, u64 offset) noexcept {
        return *reinterpret_cast<const volatile u32*>(static_cast<uintptr_t>(base + offset));
    }

    // IDR0 stage support: bit 0 S1P, bit 1 S2P.
    [[nodiscard]] inline bool stage1_supported(const capabilities& value) noexcept {
        return (value.idr0 & (1U << 0U)) != 0U;
    }
    [[nodiscard]] inline bool stage2_supported(const capabilities& value) noexcept {
        return (value.idr0 & (1U << 1U)) != 0U;
    }
    // IDR1 SIDSIZE is bits 5:0 -- the number of StreamID bits the
    // implementation supports, which bounds any future stream table.
    [[nodiscard]] inline u32 stream_id_bits(const capabilities& value) noexcept {
        return value.idr1 & 0x3fU;
    }

    /*
     * Reads identification out of a discovered SMMU. Returns false when the
     * machine has none, which is the normal case for this platform and must
     * not be treated as a failure -- QEMU's virt only instantiates one when
     * asked with `iommu=smmuv3`, and a machine legitimately without an SMMU
     * boots exactly as before.
     *
     * No write touches the device. Identification registers are read-only,
     * so this cannot disturb a machine whose SMMU some other agent owns.
     */
    [[nodiscard]] inline bool probe(const boot::fdt::range& window) noexcept {
        discovered = {};
        if (window.base == 0U || window.size == 0U)
            return false;
        discovered.base = window.base;
        discovered.idr0 = read32(discovered.base, idr0_offset);
        discovered.idr1 = read32(discovered.base, idr1_offset);
        discovered.idr5 = read32(discovered.base, idr5_offset);
        /*
         * An all-zero IDR0 means nothing answered at that address -- a
         * device tree describing an SMMU the machine did not actually
         * instantiate, or a window that reads as zero. Treated as absent
         * rather than reported as a degenerate SMMU.
         */
        discovered.present = discovered.idr0 != 0U;
        return discovered.present;
    }

    inline void report(const boot::fdt::range& window) noexcept {
        if (!probe(window)) {
            pr_info("smmu: absent (no arm,smmu-v3 node)\n");
            return;
        }
        pr_info("smmu: arm,smmu-v3 at %llx idr0=%x idr1=%x idr5=%x stage1=%u stage2=%u "
                "sidbits=%u translation=off\n",
                static_cast<unsigned long long>(discovered.base),
                static_cast<unsigned>(discovered.idr0), static_cast<unsigned>(discovered.idr1),
                static_cast<unsigned>(discovered.idr5),
                static_cast<unsigned>(stage1_supported(discovered) ? 1U : 0U),
                static_cast<unsigned>(stage2_supported(discovered) ? 1U : 0U),
                static_cast<unsigned>(stream_id_bits(discovered)));
    }
} // namespace sys::kernel::smmu
