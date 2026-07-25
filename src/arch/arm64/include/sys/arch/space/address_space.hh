#pragma once

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
    };

    inline void initialize(address_space& value) noexcept
    {
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
        memory::activate(reinterpret_cast<paddr_t>(&value.l0));
    }

    [[nodiscard]] inline constexpr vaddr_t entry() noexcept { return user_code; }
    [[nodiscard]] inline constexpr vaddr_t stack_top() noexcept
    {
        return user_stack_base + memory::page_size;
    }
} // namespace sys::arch::space
