#pragma once

#include <sys/types.hh>

/*
 * ARM SMMUv3 discovery for QEMU's arm64 virt machine (readiness checklist
 * DEV-006). Read-only: no register in this file is ever written, so this
 * driver cannot change DMA behavior for any device regardless of whether
 * an SMMUv3 is actually wired up in the QEMU invocation. Translation
 * enablement, stream tables, command/event queues, and invalidation
 * (DEV-007..010) are not implemented here.
 *
 * MMIO base/size and the devicetree compatible string are QEMU's fixed
 * `VIRT_SMMU` platform-bus entry (hw/arm/virt.c: base_memmap[VIRT_SMMU] =
 * { 0x09050000, 0x20000 }), present only when QEMU is invoked with
 * `-machine virt,...,iommu=smmuv3`. That base falls entirely inside the
 * same 2 MiB block already identity-mapped device/RW by
 * arch::memory::build_kernel_table() for the UART at 0x09000000
 * (0x09000000-0x091fffff covers 0x09050000-0x0906ffff), so no page-table
 * change is needed for this file's reads to be valid once the MMU is on.
 *
 * Register offsets and IDR0/IDR1 bit-field layouts are from ARM IHI 0070
 * ("Arm System Memory Management Unit Architecture Specification"),
 * cross-checked against Linux's
 * drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h.
 */
namespace sys::platform::smmu
{
    inline constexpr uintptr_t base = 0x09050000ULL;
    inline constexpr usize_t size = 0x00020000ULL;

    inline constexpr uintptr_t reg_idr0 = 0x0000ULL;
    inline constexpr uintptr_t reg_idr1 = 0x0004ULL;
    inline constexpr uintptr_t reg_iidr = 0x0018ULL;
    inline constexpr uintptr_t reg_aidr = 0x001cULL;

    // IDR0 bit fields (ARM IHI 0070, section 6.3.1).
    inline constexpr u32 idr0_s2p = 1U << 0U;
    inline constexpr u32 idr0_s1p = 1U << 1U;
    inline constexpr u32 idr0_cohacc = 1U << 4U;

    // IDR1 bit fields: SIDSIZE occupies bits [5:0].
    inline constexpr u32 idr1_sidsize_mask = 0x3fU;

    [[nodiscard]] inline volatile u32& reg32(uintptr_t offset) noexcept {
        return *reinterpret_cast<volatile u32*>(base + offset);
    }

    [[nodiscard]] inline u32 idr0() noexcept {
        return reg32(reg_idr0);
    }

    [[nodiscard]] inline u32 idr1() noexcept {
        return reg32(reg_idr1);
    }

    /*
     * QEMU's virt platform bus returns 0xffffffff for reads at addresses
     * with nothing wired up (the standard "unassigned access" pattern for
     * this machine), which is how an SMMU-less boot (no `iommu=smmuv3` on
     * the QEMU command line) is distinguished from a real one here. This
     * is a QEMU-specific heuristic, not a general MMIO-probing technique
     * safe on arbitrary hardware -- acceptable because this file only ever
     * targets the one explicitly-supported qemu_arm64_virt board.
     */
    [[nodiscard]] inline bool present() noexcept {
        const u32 value = idr0();
        return value != 0xffffffffU && value != 0U;
    }

    [[nodiscard]] inline bool stage1_supported() noexcept {
        return present() && (idr0() & idr0_s1p) != 0U;
    }

    [[nodiscard]] inline bool stage2_supported() noexcept {
        return present() && (idr0() & idr0_s2p) != 0U;
    }

    [[nodiscard]] inline u32 stream_id_bits() noexcept {
        return present() ? (idr1() & idr1_sidsize_mask) : 0U;
    }

    inline void initialize() noexcept {
        /*
         * Discovery only -- CR0 is never written, so any SMMUv3 QEMU has
         * wired up stays in its reset bypass/abort state and every
         * existing DMA path is unaffected by this driver's presence.
         */
    }
} // namespace sys::platform::smmu
