#pragma once

#include <sys/kernel/boot/bootinfo.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/types.hh>

namespace sys::kernel::tests::physical_memory
{
    /*
     * Confirms the platform-probed physical region table is internally
     * consistent (allocatable, page-aligned, non-overlapping, contiguous
     * bitmap offsets) and matches what was published to userspace via
     * bootinfo -- has no single [TEST] name of its own in the original
     * inline form, just a boot-time consistency gate.
     */
    [[nodiscard]] inline error_t run_layout_check() noexcept {
        using namespace sys::kernel::memory;

        if (physical_region_count == 0U || physical_region_count > maximum_physical_regions ||
            managed_pages == 0U || free_pages >= managed_pages)
            return error_t::invalid_argument;
        u64 discovered_pages{};
        for (u32 index = 0U; index < physical_region_count; ++index) {
            const auto& region = physical_regions[index];
            if (!region.allocatable || region.pages == 0U ||
                (region.base & (page_size - 1U)) != 0U || region.bitmap_offset != discovered_pages)
                return error_t::invalid_argument;
            if (index != 0U) {
                const auto& previous = physical_regions[index - 1U];
                const paddr_t previous_end =
                    previous.base + static_cast<paddr_t>(previous.pages) * page_size;
                if (previous_end > region.base)
                    return error_t::invalid_argument;
            }
            discovered_pages += region.pages;
        }
        if (discovered_pages != managed_pages ||
            boot::root_bootinfo.memory_region_count != physical_region_count ||
            boot::root_bootinfo.memory_total_pages != managed_pages ||
            boot::root_bootinfo.memory_page_size != page_size)
            return error_t::invalid_argument;
        return error_t::success;
    }
} // namespace sys::kernel::tests::physical_memory
