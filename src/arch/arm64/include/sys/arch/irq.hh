#pragma once

namespace sys::arch::irq
{
    inline void enable() noexcept
    {
        __asm__ volatile("msr daifclr, #2" ::: "memory");
    }

    inline void disable() noexcept
    {
        __asm__ volatile("msr daifset, #2" ::: "memory");
    }
} // namespace sys::arch::irq
