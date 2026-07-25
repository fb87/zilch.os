#pragma once

#include <sys/types.hh>

namespace sys::arch::hypervisor
{
    inline constexpr bool available = true;

    [[nodiscard]]
    inline error_t initialize() noexcept {
        return error_t::unsupported;
    }
} // namespace sys::arch::hypervisor
