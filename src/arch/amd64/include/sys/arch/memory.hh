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

        [[nodiscard]]
        inline error_t map_page(
            paddr_t,
            vaddr_t,
            paddr_t,
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
