#pragma once

#include <sys/types.hh>

/*
 * Extends start.S's minimal 3-entry boot GDT (null, kernel code 0x08,
 * kernel data 0x10 -- just enough to enter long mode) with user code/data
 * segments and a TSS, then loads it. Kept separate from exception.hh's IDT
 * construction: privilege-level/stack-switching setup versus exception-
 * vector setup are different concerns, and the IDT's interrupt gates
 * reference this file's kernel code selector (0x08) as their target, so
 * logically this needs to be in place first.
 *
 * Deliberately does NOT reload %cs via a far jump after `lgdt`. Indices 1
 * and 2 (kernel code/data, selectors 0x08/0x10) are byte-identical to
 * start.S's original boot GDT entries at those same indices -- the x86_64
 * architecture caches a loaded segment's descriptor state in hidden
 * register fields that are not re-validated against a new GDT until that
 * segment register is explicitly reloaded, so replacing the GDT this way
 * cannot invalidate the CS the CPU is already running with. Reloading a
 * long-mode CS safely needs a far jump or IRETQ; skipping that removes the
 * riskiest part of this file, in an environment (see this repo's
 * tools/run/run.sh) where the result cannot be executed to catch a mistake
 * -- QEMU's multiboot loader only accepts a 32-bit kernel, so this code has
 * been verified by compilation and static review only, never execution.
 */
namespace sys::arch::gdt
{
    // Selectors match what src/arch/amd64/include/sys/arch/thread/context.hh
    // and thread/entry.hh already hardcode (0x2b/0x33 = these indices'
    // byte offset OR'd with RPL 3), so those files needed no change here.
    inline constexpr u16 kernel_code_selector = 0x08U;
    inline constexpr u16 kernel_data_selector = 0x10U;
    inline constexpr u16 user_code_selector = 0x2bU;
    inline constexpr u16 user_data_selector = 0x33U;
    inline constexpr u16 tss_selector = 0x38U;

    /*
     * x86_64 TSS (AMD64 Architecture Programmer's Manual Vol. 2, 12.2.5).
     * Only rsp0 is meaningful yet: the stack the CPU switches to on a
     * ring3->ring0 transition via interrupt/exception. iopb_offset ==
     * sizeof(task_state) is the standard "no I/O permission bitmap present"
     * encoding.
     */
    struct task_state {
        u32 reserved0;
        u64 rsp0;
        u64 rsp1;
        u64 rsp2;
        u64 reserved1;
        u64 ist1;
        u64 ist2;
        u64 ist3;
        u64 ist4;
        u64 ist5;
        u64 ist6;
        u64 ist7;
        u64 reserved2;
        u16 reserved3;
        u16 iopb_offset;
    } __attribute__((packed));

    static_assert(sizeof(task_state) == 104U);

    struct table_pointer {
        u16 limit;
        u64 base;
    } __attribute__((packed));

    static_assert(sizeof(table_pointer) == 10U);

    /*
     * 9 entries: null, kernel code, kernel data, two unused (kept only so
     * the following entries land on the selectors context.hh/entry.hh
     * already hardcode), user code, user data, and the TSS descriptor
     * (16 bytes wide in long mode, occupying two slots).
     */
    inline u64 table[9]{};
    inline task_state tss{};

    /*
     * RSP0 target: the same 64 KiB stack start.S sets up for the kernel's
     * own execution (kernel.ld's .stack section), reused rather than
     * allocating a second one. This is a real limitation, not an
     * oversight: an interrupt taken while the kernel is already using this
     * stack shares it with whatever the CPU switches to on a ring3->ring0
     * transition, rather than getting an isolated stack the way a
     * production kernel would want. Correct for "make an interrupt
     * survivable at all," which is this file's actual goal; a dedicated
     * per-purpose (and eventually per-CPU) interrupt stack is future work,
     * not silently assumed away here.
     */
    extern "C" char __boot_stack_top[];

    inline void encode_tss_descriptor(u64& low, u64& high, uintptr_t base, u32 limit) noexcept {
        low = (static_cast<u64>(limit) & 0xffffULL) | ((base & 0xffffffULL) << 16U) |
              (0x89ULL << 40U) | /* present, DPL0, type=1001 (available 64-bit TSS) */
              ((static_cast<u64>(limit >> 16U) & 0xfULL) << 48U) |
              (((base >> 24U) & 0xffULL) << 56U);
        high = (base >> 32U) & 0xffffffffULL;
    }

    inline void initialize() noexcept {
        table[0] = 0U;
        table[1] = 0x00af9a000000ffffULL; /* kernel code -- byte-identical to start.S's gdt64 */
        table[2] = 0x00af92000000ffffULL; /* kernel data -- byte-identical to start.S's gdt64 */
        table[3] = 0U;
        table[4] = 0U;
        table[5] = 0x00affa000000ffffULL; /* user code, DPL3 (access byte 0x9a -> 0xfa) */
        table[6] = 0x00aff2000000ffffULL; /* user data, DPL3 (access byte 0x92 -> 0xf2) */

        tss = {};
        tss.rsp0 = reinterpret_cast<uintptr_t>(__boot_stack_top);
        tss.iopb_offset = sizeof(task_state);
        encode_tss_descriptor(table[7], table[8], reinterpret_cast<uintptr_t>(&tss),
                              sizeof(task_state) - 1U);

        const table_pointer pointer{static_cast<u16>(sizeof(table) - 1U),
                                    reinterpret_cast<u64>(&table)};
        /*
         * Selectors written as literal immediates in the template, exactly
         * like start.S's own `mov $0x10, %ax` -- not passed as "i"-
         * constraint operands. static_assert below is what keeps these in
         * sync with kernel_data_selector/tss_selector if either ever
         * changes, since the asm string can't reference the named
         * constants directly.
         */
        static_assert(kernel_data_selector == 0x10U);
        static_assert(tss_selector == 0x38U);
        __asm__ volatile("lgdt %0\n\t"
                         "mov $0x10, %%ax\n\t"
                         "mov %%ax, %%ds\n\t"
                         "mov %%ax, %%es\n\t"
                         "mov %%ax, %%ss\n\t"
                         "mov %%ax, %%fs\n\t"
                         "mov %%ax, %%gs\n\t"
                         "mov $0x38, %%ax\n\t"
                         "ltr %%ax\n\t"
                         :
                         : "m"(pointer)
                         : "ax", "memory");
    }
} // namespace sys::arch::gdt
