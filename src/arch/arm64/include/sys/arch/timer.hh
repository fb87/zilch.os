#pragma once

#include <sys/types.hh>

namespace sys::arch::timer
{
    [[nodiscard]] inline u64 counter() noexcept {
        u64 value;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
        return value;
    }

    [[nodiscard]] inline u64 frequency() noexcept {
        u64 value;
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
        return value;
    }

    inline void program(u32 ticks) noexcept {
        __asm__ volatile("msr cntv_tval_el0, %0" : : "r"(static_cast<u64>(ticks)) : "memory");
        const u64 control = 1U;
        __asm__ volatile("msr cntv_ctl_el0, %0\n\tisb" : : "r"(control) : "memory");
    }

    inline void disable() noexcept {
        const u64 control = 0U;
        __asm__ volatile("msr cntv_ctl_el0, %0\n\tisb" : : "r"(control) : "memory");
    }
} // namespace sys::arch::timer
