#pragma once

#include <sys/arch/thread/context.hh>

namespace sys::arch::thread
{
    extern "C" [[noreturn]] void sys_arm64_enter_el0(context*) noexcept;

    [[noreturn]] inline void enter_user(context& value) noexcept {
        sys_arm64_enter_el0(&value);
    }

    inline void prepare_kernel_idle(context& value, uintptr_t entry) noexcept {
        /*
         * Never reuse an interrupted user frame as an EL1 idle frame.  Build
         * a deterministic architectural frame so stale message registers or
         * user control state cannot become EL1 return state.
         */
        clear(value);
        value.instruction_pointer = entry;
        value.status = 0x5U; // EL1h
    }
} // namespace sys::arch::thread
