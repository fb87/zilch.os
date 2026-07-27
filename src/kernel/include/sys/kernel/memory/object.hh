#pragma once

#include <sys/arch/memory.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::memory
{
    enum class permission : u8 { none = 0U, read = 1U, write = 2U, execute = 4U };
    inline constexpr u32 maximum_mappings_per_frame = 8U;

    struct mapping_record {
        object::reference_t address_space{};
        vaddr_t address{};
        permission permissions{permission::none};
        capability::derivation_id_t frame_authority{};
        capability::derivation_id_t space_authority{};
        u32 generation{};
        bool valid{};
    };

    struct frame {
        object::header_t object{};
        paddr_t physical_address{};
        space_id_t owner{};
        object::reference_t owner_task{};
        u32 mapping_count{};
        bool allocated{};
        bool in_use{};
        mapping_record mappings[maximum_mappings_per_frame]{};
    };

    struct page_table {
        object::header_t object{};
        paddr_t physical_address{};
        space_id_t owner{};
        object::reference_t owner_task{};
        u8 level{};
        bool allocated{};
        bool in_use{};
    };

    [[nodiscard]] inline constexpr bool writable(permission value) noexcept {
        return (static_cast<u8>(value) & static_cast<u8>(permission::write)) != 0U;
    }
    [[nodiscard]] inline constexpr bool executable(permission value) noexcept {
        return (static_cast<u8>(value) & static_cast<u8>(permission::execute)) != 0U;
    }
    [[nodiscard]] inline constexpr bool readable(permission value) noexcept {
        return (static_cast<u8>(value) & static_cast<u8>(permission::read)) != 0U;
    }
    [[nodiscard]] inline constexpr bool valid_permission(permission value) noexcept {
        const u8 bits = static_cast<u8>(value);
        return bits != 0U && (bits & ~static_cast<u8>(0x7U)) == 0U && readable(value) &&
               !(writable(value) && executable(value));
    }
} // namespace sys::kernel::memory
