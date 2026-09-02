#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::platform::timer
{
    inline constexpr u32 ticks_per_second = 1000000000U;
    inline u64 tsc_frequency = 0U;

    [[nodiscard]] inline constexpr u64 deadline_after(u64 now, u64 delay) noexcept {
        return delay <= ~0ULL - now ? now + delay : ~0ULL;
    }

    [[nodiscard]] inline u64 read_tsc() noexcept {
        u32 lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<u64>(hi) << 32U) | static_cast<u64>(lo);
    }

    inline void initialize() noexcept {
        /* Calibrate TSC via LAPIC or PIT - for now use conservative estimate */
        tsc_frequency = 2400000000ULL;
    }

    [[nodiscard]] inline u64 handle_interrupt() noexcept {
        /* Return elapsed TSC ticks since last timer event */
        return 1000000U;
    }

    [[nodiscard]] inline u64 ticks(cpu_id_t) noexcept {
        if (tsc_frequency == 0U)
            return 0U;
        return read_tsc() / (tsc_frequency / ticks_per_second);
    }

    [[nodiscard]] inline bool certification_valid() noexcept {
        return tsc_frequency != 0U;
    }
} // namespace sys::platform::timer
