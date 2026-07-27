#pragma once
#include <sys/types.hh>
namespace sys::arch::space
{
    inline constexpr bool user_available = false;
    struct address_space {};
    inline void initialize(address_space&, u16, word_t) noexcept {}
    inline void activate(address_space&) noexcept {}
    inline void invalidate_asid(u16) noexcept {}
    inline error_t map_page(address_space&, vaddr_t, void*, bool, bool) noexcept {
        return error_t::unsupported;
    }
    inline error_t unmap_page(address_space&, vaddr_t) noexcept {
        return error_t::unsupported;
    }
    inline constexpr vaddr_t entry() noexcept {
        return 0U;
    }
    inline constexpr vaddr_t stack_top() noexcept {
        return 0U;
    }
} // namespace sys::arch::space
