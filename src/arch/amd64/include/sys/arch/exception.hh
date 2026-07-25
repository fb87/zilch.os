#pragma once

#include <sys/types.hh>

namespace sys::arch::exception
{
    inline void initialize_current_el() noexcept {}

    [[nodiscard]]
    inline u32 current_el() noexcept {
        return 0U;
    }
} // namespace sys::arch::exception
