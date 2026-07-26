#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/memory/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/space/address_space.hh>
#include <sys/types.hh>

namespace sys::kernel::memory
{
    inline constexpr u32 frame_count = 4U;
    inline constexpr u32 page_table_count = 4U;
    inline frame frames[frame_count]{};
    inline page_table page_tables[page_table_count]{};

    inline error_t initialize_objects() noexcept
    {
        for (u32 index = 0U; index < frame_count; ++index) {
            const error_t result = object::register_object(
                frames[index].object, static_cast<object_id_t>(40U + index),
                object::type_t::frame);
            if (result != error_t::success) return result;
        }
        for (u32 index = 0U; index < page_table_count; ++index) {
            page_tables[index].level = 3U;
            const error_t result = object::register_object(
                page_tables[index].object,
                static_cast<object_id_t>(44U + index),
                object::type_t::page_table);
            if (result != error_t::success) return result;
        }
        return error_t::success;
    }

    [[nodiscard]] inline error_t map(space::address_space& target,
                                     frame& source,
                                     vaddr_t address,
                                     permission permissions) noexcept
    {
        if (!valid_wx(permissions) || source.mapped) return error_t::denied;
        const error_t result = target.map_page(address, source.bytes,
                                               writable(permissions),
                                               executable(permissions));
        if (result != error_t::success) return result;
        source.mapped = true;
        source.mapped_space = target.id;
        source.mapped_address = address;
        return error_t::success;
    }

    [[nodiscard]] inline error_t unmap(space::address_space& target,
                                       frame& source) noexcept
    {
        if (!source.mapped || source.mapped_space != target.id)
            return error_t::not_found;
        const error_t result = target.unmap_page(source.mapped_address);
        if (result != error_t::success) return result;
        source.mapped = false;
        source.mapped_space = 0U;
        source.mapped_address = 0U;
        return error_t::success;
    }
}
