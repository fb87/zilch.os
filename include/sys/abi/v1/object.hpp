#pragma once

#include <sys/types.h>

namespace sys::abi::v1
{
    enum class ObjectType : u16 {
        none,
        thread,
        address_space,
        endpoint,
        notification,
        interrupt,
        frame,
        page_table,
        scheduling_context,
        virtual_machine,
        virtual_cpu
    };
} // namespace sys::abi::v1
