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
    inline constexpr irq_id_t virtual_timer_irq = 27U;

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
        reg32(sgi + 0x0100U) =
            (1U << reschedule_ipi) | (1U << tlb_shootdown_ipi) | (1U << virtual_timer_irq);
        __asm__ volatile("dsb sy" ::: "memory");

        u64 value = 1U;
        __asm__ volatile("msr ICC_SRE_EL1, %0\n\tisb" : : "r"(value) : "memory");
        value = 0xffU;
        __asm__ volatile("msr ICC_PMR_EL1, %0" : : "r"(value) : "memory");
        value = 0U;
        __asm__ volatile("msr ICC_BPR1_EL1, %0" : : "r"(value) : "memory");
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

    inline void send_ipi_all_others(irq_id_t irq) noexcept {
        const u64 mpidr = current_mpidr();
        const u64 aff1 = (mpidr >> 8U) & 0xffU;
        const u64 aff2 = (mpidr >> 16U) & 0xffU;
        const u64 aff3 = (mpidr >> 32U) & 0xffU;
        const u64 value = (aff3 << 48U) | (1ULL << 40U) | (aff2 << 32U) |
                          (static_cast<u64>(irq & 0xfU) << 24U) | (aff1 << 16U);

        __asm__ volatile("dsb ishst\n\tmsr ICC_SGI1R_EL1, %0\n\tisb" : : "r"(value) : "memory");
    }
} // namespace sys::platform::interrupt
