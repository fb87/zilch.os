#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/memory.hh>
#include <sys/types.hh>

namespace sys::arch::space
{
    inline constexpr bool user_available = true;
    inline constexpr vaddr_t user_code = 0x10000000ULL;
    inline constexpr vaddr_t user_stack_base = 0x10010000ULL;

    extern "C" char sys_arm64_user_image_start[];

    struct address_space
    {
        memory::table_t l0{};
        memory::table_t l1{};
        memory::table_t l2{};
        memory::table_t l3{};
        alignas(memory::page_size) u8 stack[memory::page_size]{};
        u16 asid{};
        volatile u32 active_cpu_mask{};
    };

    inline void initialize(address_space& value, u16 asid) noexcept
    {
        value.asid = asid;
        value.active_cpu_mask = 0U;
        memory::build_kernel_table(value.l0, value.l1, value.l2);
        const usize_t code_l2 = static_cast<usize_t>((user_code >> 21U) & 0x1ffU);
        value.l2.entry[code_l2] = memory::table_descriptor(value.l3);

        const u64 code_phys = reinterpret_cast<u64>(sys_arm64_user_image_start) & ~0xfffULL;
        value.l3.entry[(user_code >> 12U) & 0x1ffU] = code_phys
            | memory::descriptor_page | memory::access_flag
            | memory::inner_shareable | memory::attr_normal
            | memory::ap_el0_ro;

        const u64 stack_phys = reinterpret_cast<u64>(value.stack) & ~0xfffULL;
        value.l3.entry[(user_stack_base >> 12U) & 0x1ffU] = stack_phys
            | memory::descriptor_page | memory::access_flag
            | memory::inner_shareable | memory::attr_normal
            | memory::ap_el0_rw | memory::pxn | memory::uxn;
    }

    inline void activate(address_space& value) noexcept
    {
        const u64 root = reinterpret_cast<u64>(&value.l0) & 0x0000ffffffffffffULL;
        const u64 ttbr = root | (static_cast<u64>(value.asid) << 48U);
        __atomic_fetch_or(&value.active_cpu_mask,
                          1U << arch::cpu::current_id(),
                          __ATOMIC_RELEASE);
        __asm__ volatile("dsb ishst\n\tmsr ttbr0_el1, %0\n\tisb"
                         : : "r"(ttbr) : "memory");
    }

    inline void invalidate_asid(u16 asid) noexcept
    {
        const u64 operand = static_cast<u64>(asid) << 48U;
        __asm__ volatile("dsb ishst\n\ttlbi aside1is, %0\n\tdsb ish\n\tisb"
                         : : "r"(operand) : "memory");
    }

    [[nodiscard]] inline constexpr vaddr_t entry() noexcept { return user_code; }
    [[nodiscard]] inline constexpr vaddr_t stack_top() noexcept
    {
        return user_stack_base + memory::page_size;
    }
} // namespace sys::arch::space
