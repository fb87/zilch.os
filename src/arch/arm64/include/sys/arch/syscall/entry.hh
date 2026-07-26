#pragma once

#include <sys/arch/thread/context.hh>
#include <sys/types.hh>

namespace sys::arch::syscall
{
    [[nodiscard]] inline bool is_user_syscall(u64 vector, u64 syndrome) noexcept {
        return vector == 8U && ((syndrome >> 26U) & 0x3fU) == 0x15U;
    }

    [[nodiscard]] inline word_t number(const thread::context& frame) noexcept {
        return frame.x[8];
    }

    [[nodiscard]] inline word_t argument(const thread::context& frame, usize_t index) noexcept {
        return index < 8U ? frame.x[index] : 0U;
    }

    inline void set_result(thread::context& frame, word_t result) noexcept {
        frame.x[0] = result;
    }
} // namespace sys::arch::syscall
