#pragma once

#include <sys/arch/v1/types.hh>

namespace sys::arch
{
    using page_attributes_t = v1::page_attributes_t;

    namespace memory
    {
        inline constexpr usize_t page_shift = 12U;
        inline constexpr usize_t page_size = 1ULL << page_shift;
        inline constexpr usize_t virtual_address_bits = 48U;
        inline constexpr usize_t physical_address_bits = 52U;
        inline constexpr bool el0_available = false;
        inline constexpr usize_t entries = 512U;
        inline constexpr u64 descriptor_valid = 0U;
        inline constexpr u64 descriptor_page = 0U;
        inline constexpr u64 access_flag = 0U;
        inline constexpr u64 inner_shareable = 0U;
        inline constexpr u64 attr_normal = 0U;
        inline constexpr u64 ap_el0_ro = 0U;
        inline constexpr u64 ap_el0_rw = 0U;
        inline constexpr u64 pxn = 0U;
        inline constexpr u64 uxn = 0U;
        struct alignas(page_size) table_t {
            u64 entry[entries];
        };
        inline table_t kernel_l0{};
        inline table_t kernel_l1{};
        inline table_t kernel_l2{};
        inline void build_kernel_table(table_t&, table_t&, table_t&) noexcept {}
        [[nodiscard]] inline u64 table_descriptor(const table_t&) noexcept {
            return 0U;
        }
        inline void activate(paddr_t) noexcept {}
        inline void initialize() noexcept {}
        inline void initialize_cpu() noexcept {}

        [[nodiscard]] inline error_t map_page(paddr_t, vaddr_t, paddr_t,
                                              page_attributes_t) noexcept {
            return error_t::unsupported;
        }

        inline void invalidate_tlb_all() noexcept {
            word_t cr3;
            __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
            __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
        }
    } // namespace memory
} // namespace sys::arch
