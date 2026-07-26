#pragma once

#include <sys/arch/memory.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/memory/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/space/address_space.hh>
#include <sys/kernel/task/task.hh>
#include <sys/platform/memory.hh>
#include <sys/types.hh>

namespace sys::kernel::memory
{
    inline constexpr u32 bootstrap_frame_count = 4U;
    inline constexpr u32 bootstrap_page_table_count = 4U;
    inline constexpr u32 frame_count = 64U;
    inline constexpr u32 page_table_count = 32U;
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
        target.in_use = true;
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
        for (u32 index = 0U; index < bootstrap_frame_count; ++index) {
            frames[index].in_use = true;
            result = object::register_object(
                frames[index].object, static_cast<object_id_t>(40U + index), object::type_t::frame);
            if (result != error_t::success)
                return result;
            result = assign_frame(frames[index], 0U);
            if (result != error_t::success)
                return result;
        }
        for (u32 index = 0U; index < bootstrap_page_table_count; ++index) {
            page_tables[index].level = 3U;
            page_tables[index].in_use = true;
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

    [[nodiscard]] inline error_t charge_page(task::task& owner) noexcept {
        if (!owner.root && owner.memory_pages_owned >= owner.memory_quota_pages)
            return error_t::no_memory;
        ++owner.memory_pages_owned;
        return error_t::success;
    }

    inline void uncharge_page(task::task& owner) noexcept {
        if (owner.memory_pages_owned != 0U)
            --owner.memory_pages_owned;
    }

    [[nodiscard]] inline error_t create_frame(task::task& owner,
                                              capability_id_t destination) noexcept {
        if (destination >= capability::cspace_slot_count)
            return error_t::invalid_argument;
        error_t result = charge_page(owner);
        if (result != error_t::success)
            return result;
        frame* target = nullptr;
        for (u32 index = bootstrap_frame_count; index < frame_count; ++index) {
            bool expected = false;
            if (__atomic_compare_exchange_n(&frames[index].in_use, &expected, true, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                target = &frames[index];
                break;
            }
        }
        if (target == nullptr) {
            uncharge_page(owner);
            return error_t::no_memory;
        }
        result = object::register_dynamic_object(target->object, object::type_t::frame);
        if (result == error_t::success)
            result = assign_frame(*target, owner.address_space_id);
        const capability::rights_t rights{static_cast<u32>(capability::right_t::read) |
                                          static_cast<u32>(capability::right_t::write) |
                                          static_cast<u32>(capability::right_t::grant) |
                                          static_cast<u32>(capability::right_t::control)};
        if (result == error_t::success)
            result = capability::install(owner.cspace, destination,
                                         object::reference(target->object), rights);
        if (result != error_t::success) {
            if (target->allocated)
                (void)release_frame(*target);
            if (target->object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target->object));
            target->object = {};
            __atomic_store_n(&target->in_use, false, __ATOMIC_RELEASE);
            uncharge_page(owner);
        }
        return result;
    }

    [[nodiscard]] inline error_t destroy_frame(task::task& owner,
                                               capability_id_t selector) noexcept {
        object::header_t* header = nullptr;
        error_t result = capability::lookup(owner.cspace, selector, object::type_t::frame,
                                            capability::right_t::control, header);
        if (result != error_t::success)
            return result;
        frame& target = *reinterpret_cast<frame*>(header);
        if (target.object.id < object::dynamic_id_base)
            return error_t::denied;
        if (target.owner != owner.address_space_id && !owner.root)
            return error_t::denied;
        result = release_frame(target);
        if (result != error_t::success)
            return result;
        const object::reference_t reference = object::reference(target.object);
        capability::revoke_reference(reference);
        result = object::unregister_object(reference);
        if (result != error_t::success)
            return result;
        target.object = {};
        target.owner = 0U;
        __atomic_store_n(&target.in_use, false, __ATOMIC_RELEASE);
        uncharge_page(owner);
        return error_t::success;
    }

    [[nodiscard]] inline error_t create_page_table(task::task& owner, capability_id_t destination,
                                                   u8 level) noexcept {
        if (destination >= capability::cspace_slot_count || level > 3U)
            return error_t::invalid_argument;
        error_t result = charge_page(owner);
        if (result != error_t::success)
            return result;
        page_table* target = nullptr;
        for (u32 index = bootstrap_page_table_count; index < page_table_count; ++index) {
            bool expected = false;
            if (__atomic_compare_exchange_n(&page_tables[index].in_use, &expected, true, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                target = &page_tables[index];
                break;
            }
        }
        if (target == nullptr) {
            uncharge_page(owner);
            return error_t::no_memory;
        }
        result = object::register_dynamic_object(target->object, object::type_t::page_table);
        if (result == error_t::success)
            result = allocate_physical_page(target->physical_address);
        if (result == error_t::success) {
            target->owner = owner.address_space_id;
            target->level = level;
            target->allocated = true;
            const capability::rights_t rights{static_cast<u32>(capability::right_t::read) |
                                              static_cast<u32>(capability::right_t::write) |
                                              static_cast<u32>(capability::right_t::grant) |
                                              static_cast<u32>(capability::right_t::control)};
            result = capability::install(owner.cspace, destination,
                                         object::reference(target->object), rights);
        }
        if (result != error_t::success) {
            if (target->allocated)
                (void)release_physical_page(target->physical_address);
            if (target->object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target->object));
            *target = {};
            uncharge_page(owner);
        }
        return result;
    }

    [[nodiscard]] inline error_t destroy_page_table(task::task& owner,
                                                    capability_id_t selector) noexcept {
        object::header_t* header = nullptr;
        error_t result = capability::lookup(owner.cspace, selector, object::type_t::page_table,
                                            capability::right_t::control, header);
        if (result != error_t::success)
            return result;
        page_table& target = *reinterpret_cast<page_table*>(header);
        if (target.object.id < object::dynamic_id_base)
            return error_t::denied;
        if (target.owner != owner.address_space_id && !owner.root)
            return error_t::denied;
        if (!target.allocated)
            return error_t::not_found;
        result = release_physical_page(target.physical_address);
        if (result != error_t::success)
            return result;
        const object::reference_t reference = object::reference(target.object);
        capability::revoke_reference(reference);
        result = object::unregister_object(reference);
        if (result != error_t::success)
            return result;
        target = {};
        uncharge_page(owner);
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
