#pragma once
#include <sys/types.hpp>

namespace sys::platform::v1
{
    enum class MemoryKind : u8 {
        unusable,
        usable,
        reserved,
        firmware,
        device,
        kernel,
        boot_data
    };

    struct MemoryRegion final {
        paddr_t base;
        psize_t size;
        MemoryKind kind;
    };

    struct BootInfo final {
        const MemoryRegion* memory;
        usize_t memory_count;
        paddr_t firmware_data;
        cpu_id_t boot_cpu;
        u32 cpu_count;
    };
} // namespace sys::platform::v1
