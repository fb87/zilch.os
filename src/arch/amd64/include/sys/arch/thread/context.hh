#pragma once
#include <sys/arch/exception.hh>
#include <sys/types.hh>
namespace sys::arch::thread
{
    using context = exception::frame_t;
    inline void copy(context& d, const context& s) noexcept {
        d = s;
    }
    inline bool valid_user(const context&) noexcept {
        return true;
    }
    inline void initialize_user(context&, vaddr_t, vaddr_t, word_t, word_t) noexcept {}
} // namespace sys::arch::thread
