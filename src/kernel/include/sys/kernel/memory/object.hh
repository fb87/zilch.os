#pragma once

#include <sys/arch/memory.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::memory
{
    enum class permission : u8 {
        none = 0U,
        read = 1U,
        write = 2U,
        execute = 4U,
    };

    struct frame {
        object::header_t object{};
        alignas(arch::memory::page_size) u8 bytes[arch::memory::page_size]{};
        bool mapped{};
        space_id_t mapped_space{};
        vaddr_t mapped_address{};
    };

    struct page_table {
        object::header_t object{};
        space_id_t owner{};
        u8 level{};
    };

    [[nodiscard]] inline constexpr bool writable(permission value) noexcept {
        return (static_cast<u8>(value) & static_cast<u8>(permission::write)) != 0U;
    }

    [[nodiscard]] inline constexpr bool executable(permission value) noexcept {
        return (static_cast<u8>(value) & static_cast<u8>(permission::execute)) != 0U;
    }

    [[nodiscard]] inline constexpr bool valid_wx(permission value) noexcept {
        return !(writable(value) && executable(value));
    }
} // namespace sys::kernel::memory
