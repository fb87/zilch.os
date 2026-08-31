#pragma once

#include <sys/arch/syscall/entry.hh>
#include <sys/types.hh>

namespace sys::kernel::syscall
{
    inline void set_control_result(arch::thread::context& frame, error_t result) noexcept {
        arch::syscall::set_result(frame, static_cast<word_t>(static_cast<s64>(result)));
    }
} // namespace sys::kernel::syscall
