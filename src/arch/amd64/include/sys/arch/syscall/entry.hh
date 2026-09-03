#pragma once
#include <sys/arch/thread/context.hh>
#include <sys/types.hh>
namespace sys::arch::syscall
{
    inline bool is_user_syscall(u64, u64) noexcept {
        return false;
    }
    inline word_t number(const thread::context&) noexcept {
        return 0U;
    }
    inline word_t argument(const thread::context&, usize_t) noexcept {
        return 0U;
    }
    inline void set_result(thread::context&, word_t) noexcept {}

    /* No real syscall entry yet (Phase 7): nothing ever resumes a thread
     * expecting an out-parameter register to be populated. */
    inline void set_output(thread::context&, usize_t, word_t) noexcept {}
} // namespace sys::arch::syscall
