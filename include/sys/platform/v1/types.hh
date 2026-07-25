#pragma once

#include <sys/types.hh>

namespace sys::platform::v1
{
    enum class memory_region_type_t : u8 {
        unavailable,
        usable,
        reserved,
        firmware,
        device,
        kernel_image,
        boot_data,
    };

    struct memory_region_t {
        paddr_t base;
        psize_t size;
        memory_region_type_t type;
    };

    struct boot_info_t {
        paddr_t firmware_data;
        paddr_t earlyfs_base;
        psize_t earlyfs_size;
        cpu_id_t boot_cpu;
        u32 cpu_count;
    };
} // namespace sys::platform::v1
