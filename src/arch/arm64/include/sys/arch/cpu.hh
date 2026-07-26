#pragma once

#include <sys/types.hh>

namespace sys::arch::cpu
{
    inline void initialize_boot_cpu() noexcept {}

    [[nodiscard]] inline cpu_id_t current_id() noexcept {
        u64 value;
        __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
        return static_cast<cpu_id_t>(value & 0xffU);
    }

    inline void relax() noexcept {
        __asm__ volatile("yield" ::: "memory");
    }

    inline void wait_for_event() noexcept {
        __asm__ volatile("wfe" ::: "memory");
    }

    [[noreturn]] inline void halt() noexcept {
        for (;;) {
            wait_for_event();
        }
    }
} // namespace sys::arch::cpu
