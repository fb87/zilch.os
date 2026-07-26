#pragma once

#include <sys/arch/memory.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::memory
{
    enum class permission : u8 { none = 0U, read = 1U, write = 2U, execute = 4U };
    inline constexpr u32 maximum_mappings_per_frame = 8U;

    struct mapping_record {
        space_id_t space{};
        vaddr_t address{};
        permission permissions{permission::none};
        bool valid{};
    };

    struct frame {
        object::header_t object{};
        paddr_t physical_address{};
        space_id_t owner{};
        u32 mapping_count{};
        bool allocated{};
        mapping_record mappings[maximum_mappings_per_frame]{};
    };

    struct page_table {
        object::header_t object{};
        paddr_t physical_address{};
        space_id_t owner{};
        u8 level{};
        bool allocated{};
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
