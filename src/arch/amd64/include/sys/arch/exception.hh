#pragma once

#include <sys/arch/gdt.hh>
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
    extern "C" const u64 sys_amd64_vector_table[256];

    [[nodiscard]] inline u32 current_el() noexcept {
        return 0U;
    }

    /*
     * x86_64 IDT gate descriptor (AMD64 Architecture Programmer's Manual
     * Vol. 2, 4.6.3 -- "Long-Mode Interrupt Gates"). 16 bytes: the target
     * offset split across three fields plus a segment selector, an IST
     * index (0 = use the selector's own stack-switch behavior via the TSS
     * rather than a dedicated interrupt-stack-table entry -- no IST setup
     * exists yet, so this is the only valid choice), and a type/attribute
     * byte.
     */
    struct idt_entry {
        u16 offset_low;
        u16 selector;
        u8 ist;
        u8 type_attr;
        u16 offset_mid;
        u32 offset_high;
        u32 reserved;
    } __attribute__((packed));

    static_assert(sizeof(idt_entry) == 16U);

    // Present, DPL0, 64-bit interrupt gate (type 0xE). An interrupt gate
    // rather than a trap gate: it clears IF on entry, matching
    // common_handler's assumption that it runs with interrupts masked
    // while it saves registers and dispatches, the same as this codebase's
    // arm64 exception path runs with DAIF masked.
    inline constexpr u8 interrupt_gate_present_dpl0 = 0x8eU;

    inline idt_entry idt[256]{};

    inline void encode_idt_entry(idt_entry& entry, u64 handler) noexcept {
        entry.offset_low = static_cast<u16>(handler & 0xffffULL);
        entry.selector = gdt::kernel_code_selector;
        entry.ist = 0U;
        entry.type_attr = interrupt_gate_present_dpl0;
        entry.offset_mid = static_cast<u16>((handler >> 16U) & 0xffffULL);
        entry.offset_high = static_cast<u32>((handler >> 32U) & 0xffffffffULL);
        entry.reserved = 0U;
    }

    inline void initialize_idt() noexcept {
        for (u32 vector = 0U; vector < 256U; ++vector)
            encode_idt_entry(idt[vector], sys_amd64_vector_table[vector]);

        const gdt::table_pointer pointer{static_cast<u16>(sizeof(idt) - 1U),
                                         reinterpret_cast<u64>(&idt)};
        __asm__ volatile("lidt %0" : : "m"(pointer) : "memory");
    }

    inline void initialize_current_el() noexcept {
        /*
         * GDT/TSS first: the IDT's interrupt gates target
         * gdt::kernel_code_selector, so that selector must already be
         * installed (and, on a real ring3->ring0 transition, the TSS's
         * RSP0 already valid) before any interrupt can safely fire against
         * this IDT.
         */
        gdt::initialize();
        initialize_idt();
    }

    [[nodiscard]] inline u64 fault_address() noexcept {
        u64 value;
        __asm__ volatile("mov %%cr2, %0" : "=r"(value));
        return value;
    }
} // namespace sys::arch::exception
