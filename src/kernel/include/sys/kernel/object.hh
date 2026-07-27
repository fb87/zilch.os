#pragma once

#include <sys/types.hh>

namespace sys::kernel::object
{
    enum class type_t : u8 {
        none,
        task,
        thread,
        address_space,
        endpoint,
        notification,
        frame,
        page_table,
        memory_resource,
        interrupt,
        scheduling_context,
        virtual_machine,
        virtual_cpu,
    };

    struct header_t {
        object_id_t id;
        type_t type;
        u32 generation;
        u32 flags;
    };

    static_assert(sizeof(header_t) == 24U);
} // namespace sys::kernel::object
