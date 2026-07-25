#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/timer.hh>
#include <sys/types.hh>

namespace sys::platform::timer
{
    inline constexpr u32 ticks_per_second = 100U;
    inline constexpr u32 maximum_cpu_count = 4U;
    inline volatile u64 tick_count[maximum_cpu_count]{};

    [[nodiscard]] inline u32 interval_ticks() noexcept
    {
        return static_cast<u32>(arch::timer::frequency() / ticks_per_second);
    }

    inline void initialize() noexcept
    {
        arch::timer::program(interval_ticks());
    }

    [[nodiscard]] inline u64 handle_interrupt() noexcept
    {
        const cpu_id_t cpu_id = arch::cpu::current_id();
        if (cpu_id >= maximum_cpu_count) {
            arch::timer::program(interval_ticks());
            return 0U;
        }

        const u64 previous = __atomic_fetch_add(
            &tick_count[cpu_id], 1U, __ATOMIC_RELAXED);
        arch::timer::program(interval_ticks());
        return previous + 1U;
    }

    [[nodiscard]] inline u64 ticks(cpu_id_t cpu_id) noexcept
    {
        if (cpu_id >= maximum_cpu_count) {
            return 0U;
        }
        return __atomic_load_n(&tick_count[cpu_id], __ATOMIC_ACQUIRE);
    }
} // namespace sys::platform::timer
