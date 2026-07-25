#pragma once

#include <sys/arch/v1/types.hh>
#include <sys/platform/memory.hh>
#include <sys/types.hh>

namespace sys::arch
{
    using page_attributes_t = v1::page_attributes_t;

    namespace memory
    {
        inline constexpr usize_t page_shift = 12U;
        inline constexpr usize_t page_size = 1ULL << page_shift;
        inline constexpr usize_t virtual_address_bits = 48U;
        inline constexpr bool el0_available = true;
        inline constexpr usize_t physical_address_bits = 40U;
        inline constexpr u64 descriptor_valid = 1ULL;
        inline constexpr u64 descriptor_table = 3ULL;
        inline constexpr u64 descriptor_page = 3ULL;
        inline constexpr u64 access_flag = 1ULL << 10U;
        inline constexpr u64 inner_shareable = 3ULL << 8U;
        inline constexpr u64 attr_normal = 0ULL << 2U;
        inline constexpr u64 attr_device = 1ULL << 2U;
        inline constexpr u64 ap_el1_rw = 0ULL << 6U;
        inline constexpr u64 ap_el0_rw = 1ULL << 6U;
        inline constexpr u64 ap_el1_ro = 2ULL << 6U;
        inline constexpr u64 ap_el0_ro = 3ULL << 6U;
        inline constexpr u64 pxn = 1ULL << 53U;
        inline constexpr u64 uxn = 1ULL << 54U;
        inline constexpr usize_t entries = 512U;

        struct alignas(page_size) table_t { u64 entry[entries]; };
        inline table_t kernel_l0{};
        inline table_t kernel_l1{};
        inline table_t kernel_l2{};

        inline void clear(table_t& table) noexcept
        {
            for (usize_t i = 0U; i < entries; ++i) table.entry[i] = 0U;
        }

        [[nodiscard]] inline u64 table_descriptor(const table_t& table) noexcept
        {
            return (reinterpret_cast<u64>(&table) & ~0xfffULL) | descriptor_table;
        }

        inline void build_kernel_table(table_t& l0, table_t& l1, table_t& l2) noexcept
        {
            clear(l0); clear(l1); clear(l2);
            l0.entry[0] = table_descriptor(l1);
            l1.entry[0] = table_descriptor(l2);
            l1.entry[1] = 0x40000000ULL | descriptor_valid | access_flag
                | inner_shareable | attr_normal | ap_el1_rw | uxn;
            l2.entry[0x08000000ULL >> 21U] = 0x08000000ULL | descriptor_valid
                | access_flag | attr_device | ap_el1_rw | pxn | uxn;
            l2.entry[0x09000000ULL >> 21U] = 0x09000000ULL | descriptor_valid
                | access_flag | attr_device | ap_el1_rw | pxn | uxn;
        }

        inline void activate(paddr_t root) noexcept
        {
            __asm__ volatile("dsb ishst\n\tmsr ttbr0_el1, %0\n\ttlbi vmalle1is\n\tdsb ish\n\tisb"
                             : : "r"(root) : "memory");
        }

        inline void initialize_cpu() noexcept
        {
            const u64 mair = 0xffULL | (0x04ULL << 8U);
            const u64 tcr = 16ULL | (1ULL << 8U) | (1ULL << 10U)
                | (3ULL << 12U) | (1ULL << 23U) | (2ULL << 32U);
            __asm__ volatile("msr mair_el1, %0\n\tmsr tcr_el1, %1\n\tisb"
                             : : "r"(mair), "r"(tcr) : "memory");
            activate(reinterpret_cast<paddr_t>(&kernel_l0));
            u64 sctlr;
            __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
            sctlr |= 1ULL | (1ULL << 2U) | (1ULL << 12U);
            __asm__ volatile("msr sctlr_el1, %0\n\tisb" : : "r"(sctlr) : "memory");
        }

        inline void initialize() noexcept
        {
            build_kernel_table(kernel_l0, kernel_l1, kernel_l2);
            initialize_cpu();
        }

        [[nodiscard]] inline error_t map_page(paddr_t, vaddr_t, paddr_t, page_attributes_t) noexcept
        { return error_t::unsupported; }

        inline void invalidate_tlb_all() noexcept
        { __asm__ volatile("dsb ishst\n\ttlbi vmalle1is\n\tdsb ish\n\tisb" ::: "memory"); }
    } // namespace memory
} // namespace sys::arch
