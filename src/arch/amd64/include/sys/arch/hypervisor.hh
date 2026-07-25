#pragma once

#include <sys/types.hh>

namespace sys::arch::hypervisor
{
    inline constexpr bool available = true;
    inline constexpr bool active = false;

    [[nodiscard]]
    inline error_t initialize() noexcept {
        return error_t::unsupported;
    }

    [[nodiscard]] inline bool initialize_cpu() noexcept
    {
        return true;
    }

    [[nodiscard]] inline u32 verified_count() noexcept
    {
        return 0U;
    }
} // namespace sys::arch::hypervisor
