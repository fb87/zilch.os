#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    using cap_slot_t = u32;

    enum class CapabilityRight : u32
    {
        none = 0U,
        read = 1U << 0U,
        write = 1U << 1U,
        execute = 1U << 2U,
        grant = 1U << 3U
    };
} // namespace sys::abi::v1
