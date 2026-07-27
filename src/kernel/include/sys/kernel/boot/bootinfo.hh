#pragma once

#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::boot
{
    inline constexpr u32 bootinfo_magic = 0x5a494c43U; // ZILC
    inline constexpr u16 bootinfo_version = 2U;
    inline constexpr u32 initial_capability_count = 16U;
    inline constexpr u32 maximum_memory_region_count = 16U;

    struct capability_entry {
        capability_id_t selector{};
        object::reference_t object{};
        u32 rights{};
    };

    struct memory_region_entry {
        paddr_t base{};
        u64 page_count{};
        u32 flags{};
        u32 reserved{};
    };

    inline constexpr u32 memory_region_allocatable = 1U << 0U;

    struct bootinfo {
        u32 magic{bootinfo_magic};
        u16 version{bootinfo_version};
        u16 cpu_count{};
        capability_id_t root_task{};
        capability_id_t root_thread{};
        capability_id_t root_space{};
        capability_id_t root_fault_endpoint{};
        u32 capability_count{};
        capability_entry capabilities[initial_capability_count]{};
        u32 memory_region_count{};
        u32 memory_page_size{};
        u64 memory_total_pages{};
        u64 memory_free_pages{};
        memory_region_entry memory_regions[maximum_memory_region_count]{};
    };

    inline bootinfo root_bootinfo{};
} // namespace sys::kernel::boot

static_assert(sizeof(sys::kernel::boot::bootinfo) <= 4096U);
