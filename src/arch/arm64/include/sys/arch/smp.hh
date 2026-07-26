#pragma once

#include <sys/arch/cpu.hh>
#include <sys/platform/firmware.hh>
#include <sys/types.hh>

extern "C" char _secondary_start[];

namespace sys::arch::smp
{
    inline constexpr u32 maximum_cpu_count = 4U;
    inline volatile u32 online_mask = 0U;
    inline volatile u32 reschedule_ipi_count[maximum_cpu_count]{};
    inline volatile u32 tlb_shootdown_ipi_count[maximum_cpu_count]{};

    inline void mark_online() noexcept
    {
        const u32 bit = 1U << cpu::current_id();
        __atomic_fetch_or(&online_mask, bit, __ATOMIC_RELEASE);
        __asm__ volatile("sev" ::: "memory");
    }

    [[nodiscard]] inline u32 online_count() noexcept
    {
        const u32 mask = __atomic_load_n(&online_mask, __ATOMIC_ACQUIRE);
        return static_cast<u32>(__builtin_popcount(mask));
    }

    [[nodiscard]] inline bool is_online(cpu_id_t cpu_id) noexcept
    {
        if (cpu_id >= maximum_cpu_count) {
            return false;
        }
        const u32 mask = __atomic_load_n(&online_mask, __ATOMIC_ACQUIRE);
        return (mask & (1U << cpu_id)) != 0U;
    }

    [[nodiscard]] inline error_t boot_secondary_cpus() noexcept
    {
        const u32 count = platform::firmware::boot_info.cpu_count;
        if (count == 0U || count > maximum_cpu_count) {
            return error_t::invalid_argument;
        }

        for (u32 cpu_id = 1U; cpu_id < count; ++cpu_id) {
            const cpu_id_t target_mpidr = cpu_id;
            const error_t result = platform::firmware::start_cpu(
                target_mpidr,
                reinterpret_cast<uintptr_t>(_secondary_start),
                cpu_id);
            if (result != error_t::success) {
                return result;
            }
        }
        return error_t::success;
    }

    inline bool wait_until_online(u32 expected, u64 spins) noexcept
    {
        while (spins-- != 0U) {
            if (online_count() >= expected) {
                return true;
            }
            cpu::relax();
        }
        return online_count() >= expected;
    }

    inline void record_reschedule_ipi() noexcept
    {
        const cpu_id_t id = cpu::current_id();
        if (id < maximum_cpu_count) {
            __atomic_fetch_add(&reschedule_ipi_count[id], 1U, __ATOMIC_RELEASE);
        }
    }

    inline void record_tlb_shootdown_ipi() noexcept
    {
        const cpu_id_t id = cpu::current_id();
        if (id < maximum_cpu_count) {
            __atomic_fetch_add(&tlb_shootdown_ipi_count[id], 1U, __ATOMIC_RELEASE);
        }
    }

    [[nodiscard]] inline u32 reschedule_ipis(cpu_id_t id) noexcept
    {
        if (id >= maximum_cpu_count) return 0U;
        return __atomic_load_n(&reschedule_ipi_count[id], __ATOMIC_ACQUIRE);
    }

    [[nodiscard]] inline u32 tlb_shootdown_ipis(cpu_id_t id) noexcept
    {
        if (id >= maximum_cpu_count) return 0U;
        return __atomic_load_n(&tlb_shootdown_ipi_count[id], __ATOMIC_ACQUIRE);
    }
} // namespace sys::arch::smp
