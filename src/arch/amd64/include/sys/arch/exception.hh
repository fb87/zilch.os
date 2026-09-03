#pragma once

#include <sys/types.hh>

namespace sys::arch::exception
{
    struct frame_t {
        u64 rax;
        u64 rcx;
        u64 rdx;
        u64 rbx;
        u64 rbp;
        u64 rsi;
        u64 rdi;
        u64 r8;
        u64 r9;
        u64 r10;
        u64 r11;
        u64 r12;
        u64 r13;
        u64 r14;
        u64 r15;
        u64 vector;
        u64 error_code;
        u64 instruction_pointer; /* rip */
        u64 cs;
        u64 status;        /* rflags */
        u64 stack_pointer; /* rsp */
        u64 ss;
    };

    static_assert(sizeof(frame_t) == 176U);
    static_assert(__builtin_offsetof(frame_t, vector) == 120U);
    static_assert(__builtin_offsetof(frame_t, error_code) == 128U);
    static_assert(__builtin_offsetof(frame_t, instruction_pointer) == 136U);
    static_assert(__builtin_offsetof(frame_t, stack_pointer) == 160U);

    extern "C" u8 sys_amd64_vectors_start[];
    extern "C" u8 sys_amd64_vectors_end[];

    [[nodiscard]] inline u32 current_el() noexcept {
        return 0U;
    }

    inline void initialize_current_el() noexcept {
        /* Stub for Phase 3. Full IDT initialization comes later */
    }

    [[nodiscard]] inline u64 fault_address() noexcept {
        u64 value;
        __asm__ volatile("mov %%cr2, %0" : "=r"(value));
        return value;
    }
} // namespace sys::arch::exception
