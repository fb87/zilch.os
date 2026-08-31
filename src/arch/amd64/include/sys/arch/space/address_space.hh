#pragma once

#include <sys/arch/memory.hh>
#include <sys/types.hh>

namespace sys::arch::space
{
    inline constexpr bool user_available = false;
    inline constexpr vaddr_t user_code = 0U;
    inline constexpr vaddr_t kernel_identity_base = 0U;
    inline constexpr vaddr_t user_stack_base = 0U;

    inline constexpr vaddr_t user_image_end() noexcept {
        return 0U;
    }

    // No earlyfs consumer on this arch yet; nothing to validate.
    [[nodiscard]] inline bool validate_earlyfs_image() noexcept {
        return true;
    }

    struct address_space {
        memory::table_t l3{};
    };
    [[nodiscard]] inline error_t initialize(address_space&, word_t) noexcept {
        return error_t::unsupported;
    }
    inline void activate(address_space&) noexcept {}
    inline void activate_kernel() noexcept {}
    inline void release(address_space&) noexcept {}
    inline void invalidate_asid(u16) noexcept {}
    inline error_t map_page(address_space&, vaddr_t, void*, bool, bool, bool, bool) noexcept {
        return error_t::unsupported;
    }
    inline error_t unmap_page(address_space&, vaddr_t) noexcept {
        return error_t::unsupported;
    }
    inline constexpr vaddr_t entry() noexcept {
        return 0U;
    }
    inline constexpr vaddr_t entry(const address_space&) noexcept {
        return 0U;
    }
    inline constexpr vaddr_t stack_top() noexcept {
        return 0U;
    }
} // namespace sys::arch::space
