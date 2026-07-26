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

    [[nodiscard]] inline error_t configure_host() noexcept { return error_t::unsupported; }
    inline void invalidate_stage2(u16) noexcept {}
    template <typename Context, typename Exit>
    [[nodiscard]] inline error_t run_guest(u16, paddr_t, Context&, Exit&) noexcept
    {
        return error_t::unsupported;
    }

} // namespace sys::arch::hypervisor
