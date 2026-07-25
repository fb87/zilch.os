#pragma once

#include <sys/arch/v1/types.hh>
#include <sys/arch/v1/version.hh>

namespace sys::arch::v1
{
    inline constexpr usize_t minimum_page_shift = 12U;
    inline constexpr usize_t maximum_page_shift = 21U;

    [[nodiscard]]
    consteval bool valid_page_geometry(
        usize_t page_shift,
        usize_t virtual_address_bits,
        usize_t physical_address_bits) noexcept {
        return page_shift >= minimum_page_shift && page_shift <= maximum_page_shift &&
               virtual_address_bits <= 64U && physical_address_bits <= 64U;
    }
} // namespace sys::arch::v1
