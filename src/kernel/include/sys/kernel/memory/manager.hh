#pragma once

#include <sys/arch/memory.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/memory/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/space/address_space.hh>
#include <sys/platform/memory.hh>
#include <sys/types.hh>

namespace sys::kernel::memory
{
    inline constexpr u32 frame_count = 4U;
    inline constexpr u32 page_table_count = 4U;
    inline constexpr u64 page_size = arch::memory::page_size;
    inline constexpr u32 maximum_physical_pages =
        static_cast<u32>(platform::memory::ram_size / page_size);
    inline constexpr u32 bitmap_words = (maximum_physical_pages + 63U) / 64U;

    inline frame frames[frame_count]{};
    inline page_table page_tables[page_table_count]{};
    inline u64 allocation_bitmap[bitmap_words]{};
    inline paddr_t managed_base{};
    inline u32 managed_pages{};
    inline u32 free_pages{};

    extern "C" char __kernel_end[];

    inline void zero_page(paddr_t address) noexcept {
        auto* words = reinterpret_cast<volatile u64*>(static_cast<uintptr_t>(address));
        for (u32 index = 0U; index < page_size / sizeof(u64); ++index)
            words[index] = 0U;
        __asm__ volatile("dsb ishst" ::: "memory");
    }

    [[nodiscard]] inline bool page_used(u32 index) noexcept {
        return (allocation_bitmap[index / 64U] & (1ULL << (index % 64U))) != 0U;
    }
    inline void mark_page(u32 index, bool used) noexcept {
        const u64 mask = 1ULL << (index % 64U);
        if (used)
            allocation_bitmap[index / 64U] |= mask;
        else
            allocation_bitmap[index / 64U] &= ~mask;
    }

    [[nodiscard]] inline error_t initialize_physical_allocator() noexcept {
        const paddr_t kernel_end =
            (reinterpret_cast<paddr_t>(__kernel_end) + page_size - 1U) & ~(page_size - 1U);
        const paddr_t ram_end = platform::memory::ram_base + platform::memory::ram_size;
        if (kernel_end >= ram_end)
            return error_t::no_memory;
        managed_base = kernel_end;
        managed_pages = static_cast<u32>((ram_end - managed_base) / page_size);
        if (managed_pages > maximum_physical_pages)
            return error_t::invalid_argument;
        for (u32 word = 0U; word < bitmap_words; ++word)
            allocation_bitmap[word] = 0U;
        free_pages = managed_pages;
        return error_t::success;
    }

    [[nodiscard]] inline error_t allocate_physical_page(paddr_t& address) noexcept {
        address = 0U;
        for (u32 index = 0U; index < managed_pages; ++index) {
            if (page_used(index))
                continue;
            mark_page(index, true);
            --free_pages;
            address = managed_base + static_cast<paddr_t>(index) * page_size;
            zero_page(address);
            return error_t::success;
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t release_physical_page(paddr_t address) noexcept {
        if (address < managed_base || ((address - managed_base) % page_size) != 0U)
            return error_t::invalid_argument;
        const u32 index = static_cast<u32>((address - managed_base) / page_size);
        if (index >= managed_pages || !page_used(index))
            return error_t::not_found;
        zero_page(address);
        mark_page(index, false);
        ++free_pages;
        return error_t::success;
    }

    [[nodiscard]] inline error_t assign_frame(frame& target, space_id_t owner) noexcept {
        if (target.allocated)
            return error_t::busy;
        paddr_t page{};
        const error_t result = allocate_physical_page(page);
        if (result != error_t::success)
            return result;
        target.physical_address = page;
        target.owner = owner;
        target.mapping_count = 0U;
        target.allocated = true;
        for (auto& mapping : target.mappings)
            mapping = {};
        return error_t::success;
    }

    [[nodiscard]] inline error_t release_frame(frame& target) noexcept {
        if (!target.allocated)
            return error_t::not_found;
        if (target.mapping_count != 0U)
            return error_t::busy;
        const error_t result = release_physical_page(target.physical_address);
        if (result != error_t::success)
            return result;
        target.physical_address = 0U;
        target.owner = 0U;
        target.allocated = false;
        return error_t::success;
    }

    inline error_t initialize_objects() noexcept {
        error_t result = initialize_physical_allocator();
        if (result != error_t::success)
            return result;
        for (u32 index = 0U; index < frame_count; ++index) {
            result = object::register_object(
                frames[index].object, static_cast<object_id_t>(40U + index), object::type_t::frame);
            if (result != error_t::success)
                return result;
            result = assign_frame(frames[index], 0U);
            if (result != error_t::success)
                return result;
        }
        for (u32 index = 0U; index < page_table_count; ++index) {
            page_tables[index].level = 3U;
            result = object::register_object(page_tables[index].object,
                                             static_cast<object_id_t>(44U + index),
                                             object::type_t::page_table);
            if (result != error_t::success)
                return result;
            result = allocate_physical_page(page_tables[index].physical_address);
            if (result != error_t::success)
                return result;
            page_tables[index].allocated = true;
        }
        return error_t::success;
    }

    [[nodiscard]] inline error_t map(space::address_space& target, frame& source, vaddr_t address,
                                     permission permissions) noexcept {
        if (!valid_wx(permissions) || !source.allocated)
            return error_t::denied;
        for (const auto& mapping : source.mappings)
            if (mapping.valid && mapping.space == target.id && mapping.address == address)
                return error_t::busy;
        mapping_record* free_record = nullptr;
        for (auto& mapping : source.mappings)
            if (!mapping.valid) {
                free_record = &mapping;
                break;
            }
        if (free_record == nullptr)
            return error_t::no_memory;
        const error_t result = target.map_page(
            address, reinterpret_cast<void*>(static_cast<uintptr_t>(source.physical_address)),
            writable(permissions), executable(permissions));
        if (result != error_t::success)
            return result;
        *free_record = {target.id, address, permissions, true};
        ++source.mapping_count;
        return error_t::success;
    }

    [[nodiscard]] inline error_t unmap(space::address_space& target, frame& source,
                                       vaddr_t address = 0U) noexcept {
        for (auto& mapping : source.mappings) {
            if (!mapping.valid || mapping.space != target.id ||
                (address != 0U && mapping.address != address))
                continue;
            const error_t result = target.unmap_page(mapping.address);
            if (result != error_t::success)
                return result;
            mapping = {};
            --source.mapping_count;
            return error_t::success;
        }
        return error_t::not_found;
    }
} // namespace sys::kernel::memory
