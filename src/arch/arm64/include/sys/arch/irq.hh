#pragma once

#include <sys/arch/v1/types.hh>

namespace sys::arch::irq
{
    using irq_state_t = v1::irq_state_t;

    [[nodiscard]] inline irq_state_t save_and_disable() noexcept {
        word_t state;
        __asm__ volatile("mrs %0, daif\n"
                         "msr daifset, #2"
                         : "=r"(state)
                         :
                         : "memory");
        return {.value = state};
    }

    inline void restore(irq_state_t state) noexcept {
        __asm__ volatile("msr daif, %0" : : "r"(state.value) : "memory");
    }

    inline void enable() noexcept {
        __asm__ volatile("msr daifclr, #2" ::: "memory");
    }

    inline void disable() noexcept {
        __asm__ volatile("msr daifset, #2" ::: "memory");
    }

    inline void mask_all() noexcept {
        __asm__ volatile("msr daifset, #0xf" ::: "memory");
    }
} // namespace sys::arch::irq
