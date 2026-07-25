#pragma once

#include <sys/types.hh>

namespace sys::arch::exception
{
    struct frame_t
    {
        u64 x[31];
        u64 vector;
    };

    extern "C" char sys_arm64_vectors_el1[];
    extern "C" char sys_arm64_vectors_el2[];

    [[nodiscard]] inline u32 current_el() noexcept
    {
        u64 value;
        __asm__ volatile("mrs %0, CurrentEL" : "=r"(value));
        return static_cast<u32>((value >> 2U) & 0x3U);
    }

    inline void initialize_current_el() noexcept
    {
        const uintptr_t vectors = reinterpret_cast<uintptr_t>(sys_arm64_vectors_el1);
        __asm__ volatile("msr vbar_el1, %0\n\tisb" : : "r"(vectors) : "memory");
    }

    [[nodiscard]] inline u64 syndrome(u32 level) noexcept
    {
        u64 value = 0U;
        if (level == 2U) {
            __asm__ volatile("mrs %0, esr_el2" : "=r"(value));
        } else {
            __asm__ volatile("mrs %0, esr_el1" : "=r"(value));
        }
        return value;
    }

    [[nodiscard]] inline u64 fault_address(u32 level) noexcept
    {
        u64 value = 0U;
        if (level == 2U) {
            __asm__ volatile("mrs %0, far_el2" : "=r"(value));
        } else {
            __asm__ volatile("mrs %0, far_el1" : "=r"(value));
        }
        return value;
    }

    [[nodiscard]] inline u64 return_address(u32 level) noexcept
    {
        u64 value = 0U;
        if (level == 2U) {
            __asm__ volatile("mrs %0, elr_el2" : "=r"(value));
        } else {
            __asm__ volatile("mrs %0, elr_el1" : "=r"(value));
        }
        return value;
    }
} // namespace sys::arch::exception
