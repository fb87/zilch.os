#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::platform::interrupt
{
    inline constexpr uintptr_t distributor_base = 0x08000000ULL;
    inline constexpr uintptr_t redistributor_base = 0x080a0000ULL;
    inline constexpr uintptr_t redistributor_stride = 0x00020000ULL;
    inline constexpr uintptr_t sgi_base_offset = 0x00010000ULL;
    inline constexpr u32 maximum_redistributor_count = 256U;
    inline constexpr u64 register_wait_limit = 1000000ULL;
    inline constexpr irq_id_t spurious_irq = 1023U;
    inline constexpr irq_id_t reschedule_ipi = 0U;
    inline constexpr irq_id_t tlb_shootdown_ipi = 1U;
    inline constexpr irq_id_t virtual_gic_maintenance_irq = 25U;
    inline constexpr irq_id_t virtual_timer_irq = 27U;
    inline constexpr irq_id_t first_userspace_irq = 32U;
    inline constexpr irq_id_t last_userspace_irq = 1019U;

    [[nodiscard]] inline constexpr bool userspace_assignable(irq_id_t irq) noexcept {
        return irq >= first_userspace_irq && irq <= last_userspace_irq;
    }

    [[nodiscard]] inline volatile u32& reg32(uintptr_t address) noexcept {
        return *reinterpret_cast<volatile u32*>(address);
    }

    [[nodiscard]] inline volatile u64& reg64(uintptr_t address) noexcept {
        return *reinterpret_cast<volatile u64*>(address);
    }

    [[nodiscard]] inline u64 current_mpidr() noexcept {
        u64 value;
        __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
        return value;
    }

    [[nodiscard]] inline u32 current_affinity() noexcept {
        const u64 mpidr = current_mpidr();
        return static_cast<u32>((mpidr & 0xffULL) | (mpidr & 0x0000ff00ULL) |
                                (mpidr & 0x00ff0000ULL) | ((mpidr >> 8U) & 0xff000000ULL));
    }

    [[nodiscard]] inline uintptr_t find_redistributor() noexcept {
        const u32 affinity = current_affinity();

        for (u32 index = 0U; index < maximum_redistributor_count; ++index) {
            const uintptr_t base =
                redistributor_base + static_cast<uintptr_t>(index) * redistributor_stride;
            const u64 typer = reg64(base + 0x0008U);

            if (static_cast<u32>(typer >> 32U) == affinity) {
                return base;
            }

            if ((typer & (1ULL << 4U)) != 0U) {
                break;
            }
        }

        return 0U;
    }

    [[nodiscard]] inline bool wait_distributor_ready() noexcept {
        for (u64 remaining = register_wait_limit; remaining != 0U; --remaining) {
            if ((reg32(distributor_base + 0x0000U) & (1U << 31U)) == 0U) {
                return true;
            }
            arch::cpu::relax();
        }

        return false;
    }

    [[nodiscard]] inline error_t initialize_global() noexcept {
        reg32(distributor_base + 0x0000U) = 0U;
        __asm__ volatile("dsb sy" ::: "memory");
        if (!wait_distributor_ready()) {
            return error_t::timed_out;
        }

        reg32(distributor_base + 0x0000U) = (1U << 4U) | (1U << 1U);
        __asm__ volatile("dsb sy\n\tisb" ::: "memory");
        if (!wait_distributor_ready()) {
            return error_t::timed_out;
        }

        return error_t::success;
    }

    [[nodiscard]] inline error_t initialize_cpu() noexcept {
        const uintptr_t redistributor = find_redistributor();
        if (redistributor == 0U) {
            return error_t::not_found;
        }

        volatile u32& waker = reg32(redistributor + 0x0014U);
        waker = waker & ~(1U << 1U);
        __asm__ volatile("dsb sy" ::: "memory");

        u64 remaining = register_wait_limit;
        while ((waker & (1U << 2U)) != 0U) {
            if (remaining-- == 0U) {
                return error_t::timed_out;
            }
            arch::cpu::relax();
        }

        const uintptr_t sgi = redistributor + sgi_base_offset;
        reg32(sgi + 0x0080U) = 0xffffffffU;
        reg32(sgi + 0x0100U) = (1U << reschedule_ipi) | (1U << tlb_shootdown_ipi) |
                               (1U << virtual_gic_maintenance_irq) | (1U << virtual_timer_irq);
        __asm__ volatile("dsb sy" ::: "memory");

        u64 value = 1U;
        __asm__ volatile("msr ICC_SRE_EL1, %0\n\tisb" : : "r"(value) : "memory");
        value = 0xffU;
        __asm__ volatile("msr ICC_PMR_EL1, %0" : : "r"(value) : "memory");
        value = 0U;
        __asm__ volatile("msr ICC_BPR1_EL1, %0" : : "r"(value) : "memory");
        __asm__ volatile("mrs %0, ICC_CTLR_EL1" : "=r"(value));
        value |= 1U << 1U;
        __asm__ volatile("msr ICC_CTLR_EL1, %0\n\tisb" : : "r"(value) : "memory");
        value = 1U;
        __asm__ volatile("msr ICC_IGRPEN1_EL1, %0\n\tisb" : : "r"(value) : "memory");

        return error_t::success;
    }

    [[nodiscard]] inline error_t initialize() noexcept {
        const error_t global_result = initialize_global();
        if (global_result != error_t::success) {
            return global_result;
        }

        return initialize_cpu();
    }

    [[nodiscard]] inline irq_id_t acknowledge() noexcept {
        u64 value;
        __asm__ volatile("mrs %0, ICC_IAR1_EL1" : "=r"(value));
        return static_cast<irq_id_t>(value & 0x00ffffffU);
    }

    inline void complete(irq_id_t irq) noexcept {
        const u64 value = irq;
        __asm__ volatile("msr ICC_EOIR1_EL1, %0\n\tisb" : : "r"(value) : "memory");
    }

    inline void deactivate(irq_id_t irq) noexcept {
        const u64 value = irq;
        __asm__ volatile("msr ICC_DIR_EL1, %0\n\tisb" : : "r"(value) : "memory");
    }

    inline void mask(irq_id_t irq) noexcept {
        const u32 bit = 1U << (irq & 31U);
        if (irq < 32U) {
            const uintptr_t redistributor = find_redistributor();
            if (redistributor != 0U)
                reg32(redistributor + sgi_base_offset + 0x0180U) = bit;
        } else {
            reg32(distributor_base + 0x0180U + static_cast<uintptr_t>(irq / 32U) * 4U) = bit;
        }
        __asm__ volatile("dsb sy" ::: "memory");
    }

    inline void unmask(irq_id_t irq) noexcept {
        const u32 bit = 1U << (irq & 31U);
        if (irq < 32U) {
            const uintptr_t redistributor = find_redistributor();
            if (redistributor != 0U)
                reg32(redistributor + sgi_base_offset + 0x0100U) = bit;
        } else {
            reg32(distributor_base + 0x0100U + static_cast<uintptr_t>(irq / 32U) * 4U) = bit;
        }
        __asm__ volatile("dsb sy" ::: "memory");
    }

    /*
     * Shared (SPI-class) interrupts are delivered only to whichever PE(s)
     * GICD_IROUTER<n> names; the reset default is affinity 0. A userspace
     * owner of such an interrupt is not guaranteed to run on that core, so
     * this repins the interrupt to the calling core. Called from
     * acknowledge() on every ack, which keeps routing correct even if the
     * owning thread migrates, without any device- or guest-specific code.
     */
    inline void route_to_current_cpu(irq_id_t irq) noexcept {
        if (irq < 32U)
            return;
        constexpr u64 affinity_mask = 0xff00ffffffULL;
        reg64(distributor_base + 0x6000U + static_cast<uintptr_t>(irq) * 8U) =
            current_mpidr() & affinity_mask;
        __asm__ volatile("dsb sy; isb" ::: "memory");
    }

    /*
     * The reset default for GICD/GICR_IGROUPR is Group 0, but this kernel
     * only enables the Group 1 CPU interface (ICC_IGRPEN1_EL1, see
     * initialize_cpu()). A userspace-owned interrupt must be assigned to
     * Group 1 and given a priority below ICC_PMR_EL1's reset value (0xff)
     * to ever be signaled -- generic requirements for any registered IRQ,
     * not specific to any one device.
     */
    inline constexpr u8 userspace_irq_priority = 0x80U;

    [[nodiscard]] inline error_t configure(irq_id_t irq, bool edge) noexcept {
        if (!userspace_assignable(irq))
            return error_t::invalid_argument;
        const uintptr_t base =
            irq < 32U ? find_redistributor() + sgi_base_offset : distributor_base;
        if (base == 0U)
            return error_t::not_found;
        volatile u32& config = reg32(base + 0x0c00U + static_cast<uintptr_t>(irq / 16U) * 4U);
        const u32 shift = (irq & 15U) * 2U + 1U;
        const u32 value = config;
        config = edge ? value | (1U << shift) : value & ~(1U << shift);
        volatile u32& group = reg32(base + 0x0080U + static_cast<uintptr_t>(irq / 32U) * 4U);
        group |= 1U << (irq & 31U);
        *reinterpret_cast<volatile u8*>(base + 0x0400U + irq) = userspace_irq_priority;
        __asm__ volatile("dsb sy" ::: "memory");
        return error_t::success;
    }

    inline void send_ipi_all_others(irq_id_t irq) noexcept {
        const u64 mpidr = current_mpidr();
        const u64 aff1 = (mpidr >> 8U) & 0xffU;
        const u64 aff2 = (mpidr >> 16U) & 0xffU;
        const u64 aff3 = (mpidr >> 32U) & 0xffU;
        const u64 value = (aff3 << 48U) | (1ULL << 40U) | (aff2 << 32U) |
                          (static_cast<u64>(irq & 0xfU) << 24U) | (aff1 << 16U);

        __asm__ volatile("dsb ishst\n\tmsr ICC_SGI1R_EL1, %0\n\tisb" : : "r"(value) : "memory");
    }

    inline void send_ipi(cpu_id_t cpu, irq_id_t irq) noexcept {
        if (cpu >= 16U)
            return;
        const u64 mpidr = current_mpidr();
        const u64 aff1 = (mpidr >> 8U) & 0xffU;
        const u64 aff2 = (mpidr >> 16U) & 0xffU;
        const u64 aff3 = (mpidr >> 32U) & 0xffU;
        const u64 value = (aff3 << 48U) | (aff2 << 32U) | (static_cast<u64>(irq & 0xfU) << 24U) |
                          (aff1 << 16U) | (1ULL << cpu);
        __asm__ volatile("dsb ishst\n\tmsr ICC_SGI1R_EL1, %0\n\tisb" : : "r"(value) : "memory");
    }
} // namespace sys::platform::interrupt

static_assert(
    !sys::platform::interrupt::userspace_assignable(sys::platform::interrupt::reschedule_ipi));
static_assert(
    !sys::platform::interrupt::userspace_assignable(sys::platform::interrupt::virtual_timer_irq));
static_assert(sys::platform::interrupt::userspace_assignable(40U));
