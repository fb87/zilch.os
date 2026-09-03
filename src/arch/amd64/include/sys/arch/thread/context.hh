#pragma once

#include <sys/arch/exception.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/types.hh>

namespace sys::arch::thread
{
    using context = exception::frame_t;

    inline void clear(context& value) noexcept {
        value.rax = 0U;
        value.rcx = 0U;
        value.rdx = 0U;
        value.rbx = 0U;
        value.rbp = 0U;
        value.rsi = 0U;
        value.rdi = 0U;
        value.r8 = 0U;
        value.r9 = 0U;
        value.r10 = 0U;
        value.r11 = 0U;
        value.r12 = 0U;
        value.r13 = 0U;
        value.r14 = 0U;
        value.r15 = 0U;
        value.vector = 0U;
        value.error_code = 0U;
        value.instruction_pointer = 0U;
        value.cs = 0U;
        value.status = 0U;
        value.stack_pointer = 0U;
        value.ss = 0U;
    }

    inline void copy(context& destination, const context& source) noexcept {
        destination.rax = source.rax;
        destination.rcx = source.rcx;
        destination.rdx = source.rdx;
        destination.rbx = source.rbx;
        destination.rbp = source.rbp;
        destination.rsi = source.rsi;
        destination.rdi = source.rdi;
        destination.r8 = source.r8;
        destination.r9 = source.r9;
        destination.r10 = source.r10;
        destination.r11 = source.r11;
        destination.r12 = source.r12;
        destination.r13 = source.r13;
        destination.r14 = source.r14;
        destination.r15 = source.r15;
        destination.vector = source.vector;
        destination.error_code = source.error_code;
        destination.instruction_pointer = source.instruction_pointer;
        destination.cs = source.cs;
        destination.status = source.status;
        destination.stack_pointer = source.stack_pointer;
        destination.ss = source.ss;
    }

    [[nodiscard]] inline bool valid_user(const context& value) noexcept {
        constexpr vaddr_t user_code_begin = space::user_code;
        const vaddr_t user_code_end = space::user_image_end();
        constexpr vaddr_t user_stack_begin = space::user_stack_base;
        constexpr vaddr_t user_stack_end = space::user_stack_base + 0x1000ULL;

        /* RIP: must be in user code, 4-byte aligned */
        /* RSP: must be in user stack, 16-byte aligned (ABI requirement) */
        /* CS: user code segment (selector 0x2B = ring 3) */
        /* SS: user data segment (selector 0x33 = ring 3) */
        /* RFLAGS: bits 1-63 reserved, IF bit required for interrupts */
        return value.instruction_pointer >= user_code_begin &&
               value.instruction_pointer < user_code_end &&
               (value.instruction_pointer & 0x3U) == 0U &&
               value.stack_pointer >= user_stack_begin && value.stack_pointer <= user_stack_end &&
               (value.stack_pointer & 0xfU) == 0U && value.cs == 0x2bU && value.ss == 0x33U;
    }

    inline void initialize_user(context& value, vaddr_t entry, vaddr_t stack, word_t argument0,
                                word_t argument1) noexcept {
        clear(value);
        value.rdi = argument0; /* First arg in RDI */
        value.rsi = argument1; /* Second arg in RSI */
        value.stack_pointer = stack;
        value.instruction_pointer = entry;
        value.cs = 0x2bU;      /* User code segment (ring 3) */
        value.ss = 0x33U;      /* User data segment (ring 3) */
        value.status = 0x200U; /* RFLAGS: IF=1 (interrupts enabled), others reserved */
    }

    /*
     * No real syscall entry yet (Phase 7): arch::syscall::is_user_syscall()
     * is unconditionally false on this platform, so nothing ever resumes a
     * thread expecting these registers to be populated. No-ops until a real
     * SYSCALL/SYSRET convention lands to define what "the IPC result
     * register" even means here.
     */
    inline void set_ipc_result(context&, word_t) noexcept {}
    inline void set_ipc_message(context&, word_t, const word_t[4]) noexcept {}
} // namespace sys::arch::thread
