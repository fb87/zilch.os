#pragma once

#include <sys/types.hh>

namespace sys::platform::timer
{
    inline constexpr u32 ticks_per_second = 100U;
    inline void initialize() noexcept {}
    [[nodiscard]] inline u64 handle_interrupt() noexcept { return 0U; }
    [[nodiscard]] inline u64 ticks(cpu_id_t) noexcept { return 0U; }
} // namespace sys::platform::timer
