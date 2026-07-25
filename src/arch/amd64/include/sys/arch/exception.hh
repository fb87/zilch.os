#pragma once

#include <sys/types.hh>

namespace sys::arch::exception
{
    struct frame_t {
        u64 x[31]{};
        u64 vector{};
        u64 stack_pointer{};
        u64 instruction_pointer{};
        u64 status{};
    };
    inline void initialize_current_el() noexcept {}

    [[nodiscard]]
    inline u32 current_el() noexcept {
        return 0U;
    }
} // namespace sys::arch::exception
