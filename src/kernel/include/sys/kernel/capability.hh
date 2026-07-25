#pragma once

#include <sys/kernel/object.hh>

namespace sys::kernel::capability
{
    enum class right_t : u32 {
        none = 0U,
        read = 1U << 0U,
        write = 1U << 1U,
        execute = 1U << 2U,
        grant = 1U << 3U,
        control = 1U << 4U,
    };

    struct rights_t {
        u32 bits;

        [[nodiscard]]
        constexpr bool contains(right_t right) const noexcept {
            return (bits & static_cast<u32>(right)) != 0U;
        }
    };

    struct slot_t {
        object::header_t* object;
        rights_t rights;
        u32 generation;
    };

    [[nodiscard]]
    inline error_t validate(const slot_t& slot, right_t required) noexcept {
        if (slot.object == nullptr) {
            return error_t::not_found;
        }
        return slot.rights.contains(required) ? error_t::success : error_t::denied;
    }
} // namespace sys::kernel::capability
