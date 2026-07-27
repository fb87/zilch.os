#pragma once

#include <sys/types.hh>

namespace sys::arch::hardening
{
    inline void speculation_barrier() noexcept {
        __asm__ volatile("lfence" ::: "memory");
    }

    inline void initialize_cpu() noexcept {
        speculation_barrier();
    }

    inline bool inventory_valid(u32) noexcept {
        return true;
    }
} // namespace sys::arch::hardening
