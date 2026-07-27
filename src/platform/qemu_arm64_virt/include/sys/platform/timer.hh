#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/timer.hh>
#include <sys/types.hh>

namespace sys::platform::timer
{
    inline constexpr u32 ticks_per_second = 100U;
    inline constexpr u32 maximum_cpu_count = 4U;
    inline constexpr u64 maximum_interval_ticks = 0x7fffffffULL;
    inline volatile u64 tick_count[maximum_cpu_count]{};
    inline volatile u64 programmed_delta[maximum_cpu_count]{1U, 1U, 1U, 1U};

    [[nodiscard]] inline constexpr bool valid_frequency(u64 frequency) noexcept {
        return frequency >= ticks_per_second &&
               frequency / ticks_per_second <= maximum_interval_ticks;
    }

    [[nodiscard]] inline constexpr u64 deadline_after(u64 now, u64 delay) noexcept {
        return delay <= ~0ULL - now ? now + delay : ~0ULL;
    }

    [[nodiscard]] inline constexpr u64 maximum_scheduler_delta(u64 frequency) noexcept {
        return valid_frequency(frequency) ? maximum_interval_ticks / (frequency / ticks_per_second)
                                          : 1U;
    }

    [[nodiscard]] inline constexpr u64 bounded_scheduler_delta(u64 frequency, u64 delta) noexcept {
        const u64 maximum = maximum_scheduler_delta(frequency);
        return delta == 0U ? 1U : (delta < maximum ? delta : maximum);
    }

    [[nodiscard]] inline u32 interval_ticks(u64 scheduler_ticks = 1U) noexcept {
        const u64 frequency = arch::timer::frequency();
        if (!valid_frequency(frequency))
            return 1U;
        const u64 interval = frequency / ticks_per_second;
        if (scheduler_ticks > maximum_interval_ticks / interval)
            return static_cast<u32>(maximum_interval_ticks);
        return static_cast<u32>(scheduler_ticks * interval);
    }

    inline void initialize() noexcept {
        const cpu_id_t cpu_id = arch::cpu::current_id();
        if (cpu_id < maximum_cpu_count)
            __atomic_store_n(&programmed_delta[cpu_id], 1U, __ATOMIC_RELEASE);
        arch::timer::program(interval_ticks());
    }

    inline void program_deadline(cpu_id_t cpu_id, u64 deadline) noexcept {
        if (cpu_id >= maximum_cpu_count || cpu_id != arch::cpu::current_id())
            return;
        const u64 now = __atomic_load_n(&tick_count[cpu_id], __ATOMIC_ACQUIRE);
        const u64 delta =
            bounded_scheduler_delta(arch::timer::frequency(), deadline > now ? deadline - now : 1U);
        __atomic_store_n(&programmed_delta[cpu_id], delta, __ATOMIC_RELEASE);
        arch::timer::program(interval_ticks(delta));
    }

    [[nodiscard]] inline u64 handle_interrupt() noexcept {
        const cpu_id_t cpu_id = arch::cpu::current_id();
        if (cpu_id >= maximum_cpu_count) {
            arch::timer::program(interval_ticks());
            return 0U;
        }

        const u64 delta = __atomic_load_n(&programmed_delta[cpu_id], __ATOMIC_ACQUIRE);
        const u64 previous = __atomic_load_n(&tick_count[cpu_id], __ATOMIC_RELAXED);
        const u64 current = deadline_after(previous, delta);
        __atomic_store_n(&tick_count[cpu_id], current, __ATOMIC_RELEASE);
        __atomic_store_n(&programmed_delta[cpu_id], 1U, __ATOMIC_RELEASE);
        arch::timer::program(interval_ticks());
        return current;
    }

    [[nodiscard]] inline u64 ticks(cpu_id_t cpu_id) noexcept {
        if (cpu_id >= maximum_cpu_count) {
            return 0U;
        }
        return __atomic_load_n(&tick_count[cpu_id], __ATOMIC_ACQUIRE);
    }

    [[nodiscard]] inline bool certification_valid() noexcept {
        const u64 frequency = arch::timer::frequency();
        return valid_frequency(frequency) && interval_ticks() != 0U &&
               bounded_scheduler_delta(frequency, ~0ULL) != 0U;
    }
} // namespace sys::platform::timer

static_assert(sys::platform::timer::valid_frequency(100U));
static_assert(!sys::platform::timer::valid_frequency(99U));
static_assert(!sys::platform::timer::valid_frequency(~0ULL));
static_assert(sys::platform::timer::deadline_after(~0ULL - 1U, 2U) == ~0ULL);
static_assert(sys::platform::timer::bounded_scheduler_delta(100U, 0U) == 1U);
