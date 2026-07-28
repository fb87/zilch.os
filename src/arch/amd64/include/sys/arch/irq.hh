#pragma once

#include <sys/arch/v1/types.hh>

namespace sys::arch::irq
{
    using irq_state_t = v1::irq_state_t;
    [[nodiscard]] inline irq_state_t save_and_disable() noexcept {
        word_t flags;
        __asm__ volatile("pushfq\npopq %0\ncli" : "=r"(flags)::"memory");
        return {.value = flags};
    }

    inline void restore(irq_state_t state) noexcept {
        __asm__ volatile("pushq %0\npopfq" : : "r"(state.value) : "memory", "cc");
    }

    inline void disable() noexcept {
        __asm__ volatile("cli" ::: "memory");
    }

    inline void mask_all() noexcept {
        disable();
    }

    inline void enable() noexcept {
        __asm__ volatile("sti" ::: "memory");
    }
} // namespace sys::arch::irq
