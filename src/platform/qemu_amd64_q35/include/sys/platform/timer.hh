#pragma once

#include <sys/types.hh>

namespace sys::platform::timer
{
    inline constexpr u32 ticks_per_second = 100U;
    [[nodiscard]] inline constexpr u64 deadline_after(u64 now, u64 delay) noexcept {
        return delay <= ~0ULL - now ? now + delay : ~0ULL;
    }
    inline void initialize() noexcept {}
    [[nodiscard]] inline u64 handle_interrupt() noexcept {
        return 0U;
    }
    [[nodiscard]] inline u64 ticks(cpu_id_t) noexcept {
        return 0U;
    }
    [[nodiscard]] inline bool certification_valid() noexcept {
        return true;
    }
} // namespace sys::platform::timer
