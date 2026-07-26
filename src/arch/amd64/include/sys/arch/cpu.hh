#pragma once

#include <sys/types.hh>

namespace sys::arch::cpu
{
    inline void initialize_boot_cpu() noexcept {}

    [[nodiscard]] inline cpu_id_t current_id() noexcept {
        return 0U;
    }

    inline void relax() noexcept {
        __asm__ volatile("pause" ::: "memory");
    }

    inline void wait_for_event() noexcept {
        __asm__ volatile("hlt" ::: "memory");
    }

    [[noreturn]] inline void halt() noexcept {
        __asm__ volatile("cli" ::: "memory");
        for (;;) {
            wait_for_event();
        }
    }
} // namespace sys::arch::cpu
