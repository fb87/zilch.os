#pragma once

#include <sys/types.hh>

namespace sys::arch::timer
{
    [[nodiscard]]
    inline u64 counter() noexcept {
        u32 low;
        u32 high;
        __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
        return (static_cast<u64>(high) << 32U) | low;
    }

    [[nodiscard]]
    inline u64 frequency() noexcept {
        return 0U;
    }
} // namespace sys::arch::timer
