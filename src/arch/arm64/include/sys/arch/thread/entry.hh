#pragma once

#include <sys/arch/thread/context.hh>

namespace sys::arch::thread
{
    extern "C" [[noreturn]] void sys_arm64_enter_el0(context*) noexcept;

    [[noreturn]] inline void enter_user(context& value) noexcept
    {
        sys_arm64_enter_el0(&value);
    }
} // namespace sys::arch::thread
