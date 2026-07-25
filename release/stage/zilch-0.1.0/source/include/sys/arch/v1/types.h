#pragma once
#include <sys/types.h>

namespace sys::arch::v1
{
    enum class PrivilegeLevel : u8
    {
        monitor = 0U,
        hypervisor = 1U,
        kernel = 2U,
        user = 3U
    };
    enum class Feature : u64
    {
        none = 0,
        smp = 1ULL << 0,
        user = 1ULL << 1,
        virtualization = 1ULL << 2,
        stage2 = 1ULL << 3
    };

    struct Capabilities final
    {
        u64 features;
        u8 register_bits;
        u8 virtual_address_bits;
        u8 physical_address_bits;
        u8 reserved;
    };
    struct UserContext;
} // namespace sys::arch::v1
