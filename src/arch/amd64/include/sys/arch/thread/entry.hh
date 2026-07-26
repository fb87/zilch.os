#pragma once
#include <sys/arch/thread/context.hh>
namespace sys::arch::thread
{
    [[noreturn]] inline void enter_user(context&) noexcept {
        for (;;)
            __asm__ volatile("hlt");
    }
    inline void prepare_kernel_idle(context&, uintptr_t) noexcept {}
} // namespace sys::arch::thread
