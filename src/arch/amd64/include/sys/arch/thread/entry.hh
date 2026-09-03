#pragma once

#include <sys/arch/thread/context.hh>

namespace sys::arch::thread
{
    /* No real syscall entry yet (Phase 7): a user thread that ran here would
     * fault the instant it issued a syscall, since EFER.SCE/STAR/LSTAR are
     * unprogrammed and arch::syscall::is_user_syscall() is unconditionally
     * false. Gates sys::kernel::thread::enter_first_user_thread() to park in
     * the kernel idle loop instead of ever attempting real user entry. */
    inline constexpr bool user_entry_ready = false;

    extern "C" [[noreturn]] void sys_amd64_enter_user(context*) noexcept;

    [[noreturn]] inline void enter_user(context& value) noexcept {
        sys_amd64_enter_user(&value);
    }

    inline void prepare_kernel_idle(context& value, uintptr_t entry) noexcept {
        /* Kernel idle frame: no user mode context */
        clear(value);
        value.instruction_pointer = entry;
        value.cs = 0x08U;      /* Kernel code segment */
        value.ss = 0x10U;      /* Kernel data segment */
        value.status = 0x200U; /* RFLAGS: IF=1 */
    }
} // namespace sys::arch::thread
