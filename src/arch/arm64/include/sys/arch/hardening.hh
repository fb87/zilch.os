#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::arch::hardening
{
    struct features {
        bool csv2{};
        bool csv3{};
        bool ssbs{};
        bool pointer_authentication{};
        bool branch_target_identification{};
    };

    inline features cpu_features[64U]{};
    inline bool cpu_initialized[64U]{};

    [[nodiscard]] inline features detect() noexcept {
        u64 pfr0{};
        u64 pfr1{};
        u64 isar1{};
        __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r"(pfr0));
        __asm__ volatile("mrs %0, id_aa64pfr1_el1" : "=r"(pfr1));
        __asm__ volatile("mrs %0, id_aa64isar1_el1" : "=r"(isar1));
        return {
            .csv2 = ((pfr0 >> 56U) & 0xfU) != 0U,
            .csv3 = ((pfr0 >> 60U) & 0xfU) != 0U,
            .ssbs = ((pfr1 >> 4U) & 0xfU) != 0U,
            .pointer_authentication =
                (((isar1 >> 4U) & 0xfU) != 0U) || (((isar1 >> 8U) & 0xfU) != 0U),
            .branch_target_identification = (pfr1 & 0xfU) != 0U,
        };
    }

    inline void speculation_barrier() noexcept {
        /*
         * CSDB is an architectural HINT on older cores and therefore safe on
         * the Armv8-A baseline. Pair it with a context-synchronizing ISB at
         * privilege-domain transitions.
         */
        __asm__ volatile(".inst 0xd503229f\n\tisb" ::: "memory");
    }

    inline void initialize_cpu() noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu < 64U) {
            cpu_features[cpu] = detect();
            __atomic_store_n(&cpu_initialized[cpu], true, __ATOMIC_RELEASE);
        }
        speculation_barrier();
    }

    [[nodiscard]] inline bool inventory_valid(u32 online_cpus) noexcept {
        if (online_cpus == 0U || online_cpus > 64U)
            return false;
        for (u32 cpu = 0U; cpu < online_cpus; ++cpu) {
            if (!__atomic_load_n(&cpu_initialized[cpu], __ATOMIC_ACQUIRE))
                return false;
        }
        return true;
    }
} // namespace sys::arch::hardening
