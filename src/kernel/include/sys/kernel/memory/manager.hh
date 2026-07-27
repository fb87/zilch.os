#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/memory.hh>
#include <sys/kernel/boot/fdt.hh>
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
    inline constexpr u32 resource_count = 32U;
    inline constexpr u32 extent_node_count = 256U;
    inline constexpr u64 page_size = arch::memory::page_size;
    inline constexpr u32 maximum_physical_pages =
        static_cast<u32>(platform::memory::ram_size / page_size);
    inline constexpr u32 bitmap_words = (maximum_physical_pages + 63U) / 64U;

    inline frame frames[frame_count]{};
    inline page_table page_tables[page_table_count]{};
    inline resource resources[resource_count]{};
    struct extent_node {
        paddr_t base{};
        u32 pages{};
        u32 next{invalid_extent_index};
        bool in_use{};
    };
    inline extent_node extent_nodes[extent_node_count]{};
    inline u64 allocation_bitmap[bitmap_words]{};
    inline paddr_t managed_base{};
    inline u32 managed_pages{};
    inline u32 free_pages{};
    inline volatile u32 allocator_lock{};
    inline volatile u32 mapping_lock{};
    inline u32 next_mapping_generation{1U};
    inline uintptr_t firmware_data{};

    enum class inventory_source : u8 {
        firmware_register = 0U,
        platform_probe = 1U,
        platform_fallback = 2U,
    };
    inline inventory_source physical_inventory_source{inventory_source::firmware_register};

    struct physical_region {
        paddr_t base{};
        u32 pages{};
        bool allocatable{};
        u32 bitmap_offset{};
    };

    inline constexpr u32 maximum_physical_regions = 16U;
    inline physical_region physical_regions[maximum_physical_regions]{};
    inline u32 physical_region_count{};

    inline void lock_allocator() noexcept {
        while (__atomic_exchange_n(&allocator_lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
    }

    inline void unlock_allocator() noexcept {
        __atomic_store_n(&allocator_lock, 0U, __ATOMIC_RELEASE);
    }

    inline void lock_mappings() noexcept {
        while (__atomic_exchange_n(&mapping_lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
    }

    inline void unlock_mappings() noexcept {
        __atomic_store_n(&mapping_lock, 0U, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline constexpr bool same_reference(const object::reference_t& left,
                                                       const object::reference_t& right) noexcept {
        return left.id == right.id && left.generation == right.generation &&
               left.type == right.type;
    }

    [[nodiscard]] inline constexpr bool
    valid_reference(const object::reference_t& reference) noexcept {
        return reference.type != object::type_t::none && reference.generation != 0U;
    }

    inline void clear_resource(resource& value) noexcept {
        value.object = {};
        value.owner_task = {};
        value.parent = {};
        value.quota_pages = 0U;
        value.used_pages = 0U;
        value.delegated_pages = 0U;
        u32 extent = value.extent_head;
        while (extent != invalid_extent_index) {
            const u32 next = extent_nodes[extent].next;
            extent_nodes[extent] = {};
            extent_nodes[extent].next = invalid_extent_index;
            extent = next;
        }
        value.extent_head = invalid_extent_index;
        value.extent_count = 0U;
        value.root = false;
        value.in_use = false;
    }

    inline void clear_frame(frame& value) noexcept {
        value.object = {};
        value.physical_address = 0U;
        value.owner = 0U;
        value.owner_task = {};
        value.resource_authority = {};
        value.mapping_count = 0U;
        value.allocated = false;
        value.device = false;
        value.in_use = false;
        for (auto& mapping : value.mappings)
            mapping = {};
    }

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

    [[nodiscard]] inline constexpr paddr_t align_up(paddr_t value) noexcept {
        return (value + page_size - 1U) & ~(page_size - 1U);
    }

    [[nodiscard]] inline constexpr paddr_t align_down(paddr_t value) noexcept {
        return value & ~(page_size - 1U);
    }

    inline error_t append_allocatable_region(paddr_t base, paddr_t end) noexcept {
        base = align_up(base);
        end = align_down(end);
        if (base >= end)
            return error_t::success;
        const u64 pages64 = (end - base) / page_size;
        if (pages64 > maximum_physical_pages || managed_pages + pages64 > maximum_physical_pages ||
            physical_region_count >= maximum_physical_regions)
            return error_t::no_memory;
        physical_regions[physical_region_count++] = {base, static_cast<u32>(pages64), true,
                                                     managed_pages};
        managed_pages += static_cast<u32>(pages64);
        return error_t::success;
    }

    inline error_t
    append_memory_minus_reservations(paddr_t base, psize_t size,
                                     const boot::fdt::inventory& inventory) noexcept {
        if (size == 0U || base + size < base)
            return error_t::invalid_argument;
        struct segment {
            paddr_t base{};
            paddr_t end{};
        };
        segment active[maximum_physical_regions]{{base, base + size}};
        u32 active_count = 1U;
        for (u32 reserved_index = 0U; reserved_index < inventory.reserved_count; ++reserved_index) {
            const auto& reserved = inventory.reserved[reserved_index];
            if (reserved.size == 0U || reserved.base + reserved.size < reserved.base)
                return error_t::invalid_argument;
            const paddr_t reserved_begin = align_down(reserved.base);
            const paddr_t reserved_end = align_up(reserved.base + reserved.size);
            segment next[maximum_physical_regions]{};
            u32 next_count{};
            for (u32 index = 0U; index < active_count; ++index) {
                const segment current = active[index];
                if (reserved_end <= current.base || reserved_begin >= current.end) {
                    if (next_count >= maximum_physical_regions)
                        return error_t::no_memory;
                    next[next_count++] = current;
                    continue;
                }
                if (reserved_begin > current.base) {
                    if (next_count >= maximum_physical_regions)
                        return error_t::no_memory;
                    next[next_count++] = {current.base, reserved_begin};
                }
                if (reserved_end < current.end) {
                    if (next_count >= maximum_physical_regions)
                        return error_t::no_memory;
                    next[next_count++] = {reserved_end, current.end};
                }
            }
            active_count = next_count;
            for (u32 index = 0U; index < active_count; ++index)
                active[index] = next[index];
        }
        for (u32 index = 0U; index < active_count; ++index) {
            const error_t result = append_allocatable_region(active[index].base, active[index].end);
            if (result != error_t::success)
                return result;
        }
        return error_t::success;
    }

    [[nodiscard]] inline error_t initialize_physical_allocator() noexcept {
        for (u32 word = 0U; word < bitmap_words; ++word)
            allocation_bitmap[word] = 0U;
        for (auto& region : physical_regions)
            region = {};
        physical_region_count = 0U;
        managed_pages = 0U;
        managed_base = 0U;

        boot::fdt::inventory inventory{};
        physical_inventory_source = inventory_source::firmware_register;
        error_t parse_result = boot::fdt::parse(firmware_data, inventory);
#if defined(__aarch64__)
        if (parse_result != error_t::success &&
            firmware_data != static_cast<uintptr_t>(platform::memory::ram_base)) {
            // QEMU virt places its generated DTB at the start of RAM for the
            // raw -kernel boot path, even when x0 is not preserved by a
            // particular loader configuration. Probe that conventional
            // location before using the explicit platform fallback.
            parse_result =
                boot::fdt::parse(static_cast<uintptr_t>(platform::memory::ram_base), inventory);
            if (parse_result == error_t::success)
                physical_inventory_source = inventory_source::platform_probe;
        }
#endif
        if (parse_result != error_t::success) {
            inventory.memory_count = 0U;
            inventory.reserved_count = 0U;
            inventory.blob_base = 0U;
            inventory.blob_size = 0U;
            for (auto& item : inventory.memory)
                item = {};
            for (auto& item : inventory.reserved)
                item = {};
            inventory.memory[0] = {platform::memory::ram_base, platform::memory::ram_size};
            inventory.memory_count = 1U;
            physical_inventory_source = inventory_source::platform_fallback;
        }

        const paddr_t kernel_begin = platform::memory::ram_base;
        const paddr_t kernel_end = align_up(reinterpret_cast<paddr_t>(__kernel_end));
        if (!boot::fdt::append(inventory.reserved, boot::fdt::inventory::maximum_reserved_ranges,
                               inventory.reserved_count, kernel_begin, kernel_end - kernel_begin))
            return error_t::no_memory;

        for (u32 index = 0U; index < inventory.memory_count; ++index) {
            const error_t result = append_memory_minus_reservations(
                inventory.memory[index].base, inventory.memory[index].size, inventory);
            if (result != error_t::success)
                return result;
        }
        if (physical_region_count == 0U || managed_pages == 0U)
            return error_t::no_memory;
        managed_base = physical_regions[0].base;
        __atomic_store_n(&free_pages, managed_pages, __ATOMIC_RELEASE);
        return error_t::success;
    }

    [[nodiscard]] inline u32 allocate_extent_node() noexcept {
        for (u32 index = 0U; index < extent_node_count; ++index) {
            if (!extent_nodes[index].in_use) {
                extent_nodes[index] = {};
                extent_nodes[index].next = invalid_extent_index;
                extent_nodes[index].in_use = true;
                return index;
            }
        }
        return invalid_extent_index;
    }

    inline void release_extent_node(u32 index) noexcept {
        if (index >= extent_node_count)
            return;
        extent_nodes[index] = {};
        extent_nodes[index].next = invalid_extent_index;
    }

    inline void release_resource_extents(resource& value) noexcept {
        u32 current = value.extent_head;
        while (current != invalid_extent_index) {
            const u32 next = extent_nodes[current].next;
            release_extent_node(current);
            current = next;
        }
        value.extent_head = invalid_extent_index;
        value.extent_count = 0U;
    }

    [[nodiscard]] inline bool address_in_resource_extent(const resource& value,
                                                         paddr_t address) noexcept {
        for (u32 current = value.extent_head; current != invalid_extent_index;
             current = extent_nodes[current].next) {
            const auto& extent = extent_nodes[current];
            const paddr_t end = extent.base + static_cast<paddr_t>(extent.pages) * page_size;
            if (address >= extent.base && address < end)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool address_delegated_to_child(paddr_t address) noexcept {
        for (const auto& value : resources) {
            if (!__atomic_load_n(&value.in_use, __ATOMIC_ACQUIRE) || value.root)
                continue;
            if (address_in_resource_extent(value, address))
                return true;
        }
        return false;
    }

    [[nodiscard]] inline error_t allocate_resource_page(resource& authority,
                                                        paddr_t& address) noexcept {
        address = 0U;
        lock_allocator();
        for (u32 current = authority.extent_head; current != invalid_extent_index;
             current = extent_nodes[current].next) {
            const auto& extent = extent_nodes[current];
            for (u32 page = 0U; page < extent.pages; ++page) {
                const paddr_t candidate = extent.base + static_cast<paddr_t>(page) * page_size;
                for (u32 region_index = 0U; region_index < physical_region_count; ++region_index) {
                    const auto& region = physical_regions[region_index];
                    const paddr_t end =
                        region.base + static_cast<paddr_t>(region.pages) * page_size;
                    if (candidate < region.base || candidate >= end)
                        continue;
                    const u32 bitmap_index =
                        region.bitmap_offset +
                        static_cast<u32>((candidate - region.base) / page_size);
                    if (page_used(bitmap_index))
                        break;
                    mark_page(bitmap_index, true);
                    __atomic_fetch_sub(&free_pages, 1U, __ATOMIC_ACQ_REL);
                    address = candidate;
                    unlock_allocator();
                    zero_page(address);
                    return error_t::success;
                }
            }
        }
        unlock_allocator();
        return error_t::no_memory;
    }

    inline void insert_extent_node_sorted(resource& value, u32 node_index) noexcept {
        auto& incoming = extent_nodes[node_index];
        u32 previous = invalid_extent_index;
        u32 current = value.extent_head;
        while (current != invalid_extent_index && extent_nodes[current].base < incoming.base) {
            previous = current;
            current = extent_nodes[current].next;
        }
        incoming.next = current;
        if (previous == invalid_extent_index)
            value.extent_head = node_index;
        else
            extent_nodes[previous].next = node_index;
        ++value.extent_count;

        if (previous != invalid_extent_index) {
            auto& left = extent_nodes[previous];
            if (left.base + static_cast<paddr_t>(left.pages) * page_size == incoming.base) {
                left.pages += incoming.pages;
                left.next = incoming.next;
                release_extent_node(node_index);
                node_index = previous;
                --value.extent_count;
            }
        }
        auto& merged = extent_nodes[node_index];
        const u32 right_index = merged.next;
        if (right_index != invalid_extent_index) {
            auto& right = extent_nodes[right_index];
            if (merged.base + static_cast<paddr_t>(merged.pages) * page_size == right.base) {
                merged.pages += right.pages;
                merged.next = right.next;
                release_extent_node(right_index);
                --value.extent_count;
            }
        }
    }

    [[nodiscard]] inline bool append_resource_extent(resource& value, paddr_t base,
                                                     u32 pages) noexcept {
        if (pages == 0U)
            return true;
        const u32 node = allocate_extent_node();
        if (node == invalid_extent_index)
            return false;
        extent_nodes[node].base = base;
        extent_nodes[node].pages = pages;
        insert_extent_node_sorted(value, node);
        return true;
    }

    [[nodiscard]] inline error_t carve_resource_extents(resource& parent, resource& child,
                                                        u32 pages) noexcept {
        if (pages == 0U)
            return error_t::invalid_argument;
        u32 remaining = pages;
        child.extent_head = invalid_extent_index;
        child.extent_count = 0U;
        while (remaining != 0U) {
            u32 previous = invalid_extent_index;
            u32 current = parent.extent_head;
            if (current == invalid_extent_index)
                break;
            while (extent_nodes[current].next != invalid_extent_index) {
                previous = current;
                current = extent_nodes[current].next;
            }
            auto& tail = extent_nodes[current];
            const u32 take = tail.pages < remaining ? tail.pages : remaining;
            if (take == tail.pages) {
                if (previous == invalid_extent_index)
                    parent.extent_head = invalid_extent_index;
                else
                    extent_nodes[previous].next = invalid_extent_index;
                --parent.extent_count;
                tail.next = invalid_extent_index;
                insert_extent_node_sorted(child, current);
            } else {
                const u32 node = allocate_extent_node();
                if (node == invalid_extent_index)
                    break;
                extent_nodes[node].base =
                    tail.base + static_cast<paddr_t>(tail.pages - take) * page_size;
                extent_nodes[node].pages = take;
                tail.pages -= take;
                insert_extent_node_sorted(child, node);
            }
            remaining -= take;
        }
        if (remaining == 0U)
            return error_t::success;
        while (child.extent_head != invalid_extent_index) {
            const u32 node = child.extent_head;
            child.extent_head = extent_nodes[node].next;
            --child.extent_count;
            extent_nodes[node].next = invalid_extent_index;
            insert_extent_node_sorted(parent, node);
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t allocate_physical_page(paddr_t& address) noexcept {
        address = 0U;
        lock_allocator();
        for (u32 region_index = 0U; region_index < physical_region_count; ++region_index) {
            const auto& region = physical_regions[region_index];
            for (u32 page = 0U; page < region.pages; ++page) {
                const u32 bitmap_index = region.bitmap_offset + page;
                if (page_used(bitmap_index) ||
                    address_delegated_to_child(region.base +
                                               static_cast<paddr_t>(page) * page_size))
                    continue;
                mark_page(bitmap_index, true);
                __atomic_fetch_sub(&free_pages, 1U, __ATOMIC_ACQ_REL);
                address = region.base + static_cast<paddr_t>(page) * page_size;
                unlock_allocator();
                zero_page(address);
                return error_t::success;
            }
        }
        unlock_allocator();
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t release_physical_page(paddr_t address) noexcept {
        if ((address & (page_size - 1U)) != 0U)
            return error_t::invalid_argument;
        lock_allocator();
        for (u32 region_index = 0U; region_index < physical_region_count; ++region_index) {
            const auto& region = physical_regions[region_index];
            const paddr_t end = region.base + static_cast<paddr_t>(region.pages) * page_size;
            if (address < region.base || address >= end)
                continue;
            const u32 page = static_cast<u32>((address - region.base) / page_size);
            const u32 bitmap_index = region.bitmap_offset + page;
            if (!page_used(bitmap_index)) {
                unlock_allocator();
                return error_t::not_found;
            }
            zero_page(address);
            mark_page(bitmap_index, false);
            __atomic_fetch_add(&free_pages, 1U, __ATOMIC_ACQ_REL);
            unlock_allocator();
            return error_t::success;
        }
        unlock_allocator();
        return error_t::invalid_argument;
    }

    [[nodiscard]] inline error_t assign_frame(frame& target, space_id_t owner,
                                              object::reference_t owner_task = {},
                                              resource* authority = nullptr) noexcept {
        if (target.allocated)
            return error_t::busy;
        paddr_t page{};
        const error_t result = authority == nullptr ? allocate_physical_page(page)
                                                    : allocate_resource_page(*authority, page);
        if (result != error_t::success)
            return result;
        target.physical_address = page;
        target.owner = owner;
        target.owner_task = owner_task;
        target.mapping_count = 0U;
        target.allocated = true;
        target.device = false;
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
        const error_t result =
            target.device ? error_t::success : release_physical_page(target.physical_address);
        if (result != error_t::success)
            return result;
        target.physical_address = 0U;
        target.owner = 0U;
        target.owner_task = {};
        target.allocated = false;
        target.device = false;
        return error_t::success;
    }

    [[nodiscard]] inline error_t create_resource(task::task& owner, capability_id_t destination,
                                                 u32 quota_pages, object::reference_t parent = {},
                                                 bool root_resource = false) noexcept {
        if (destination >= capability::cspace_slot_count || quota_pages == 0U)
            return error_t::invalid_argument;
        resource* target = nullptr;
        for (u32 index = 0U; index < resource_count; ++index) {
            bool expected = false;
            if (__atomic_compare_exchange_n(&resources[index].in_use, &expected, true, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                target = &resources[index];
                break;
            }
        }
        if (target == nullptr)
            return error_t::no_memory;
        error_t result =
            root_resource
                ? object::register_object(target->object,
                                          object::bootstrap_id::root_memory_resource,
                                          object::type_t::memory_resource)
                : object::register_dynamic_object(target->object, object::type_t::memory_resource);
        if (result == error_t::success) {
            target->owner_task = object::reference(owner.object);
            target->parent = parent;
            target->quota_pages = quota_pages;
            target->used_pages = 0U;
            target->delegated_pages = 0U;
            target->extent_head = invalid_extent_index;
            target->extent_count = 0U;
            if (root_resource) {
                for (u32 index = 0U; index < physical_region_count; ++index) {
                    if (!append_resource_extent(*target, physical_regions[index].base,
                                                physical_regions[index].pages)) {
                        result = error_t::no_memory;
                        break;
                    }
                }
            }
            target->root = root_resource;
            const capability::rights_t rights{static_cast<u32>(capability::right_t::read) |
                                              static_cast<u32>(capability::right_t::write) |
                                              static_cast<u32>(capability::right_t::grant) |
                                              static_cast<u32>(capability::right_t::control)};
            if (result == error_t::success)
                result = capability::install(owner.cspace, destination,
                                             object::reference(target->object), rights);
        }
        if (result != error_t::success) {
            if (target->object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target->object));
            clear_resource(*target);
        }
        return result;
    }

    [[nodiscard]] inline error_t charge_resource(resource& value) noexcept {
        u32 used = __atomic_load_n(&value.used_pages, __ATOMIC_ACQUIRE);
        for (;;) {
            if (used >= value.quota_pages)
                return error_t::no_memory;
            if (__atomic_compare_exchange_n(&value.used_pages, &used, used + 1U, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                return error_t::success;
        }
    }

    inline void uncharge_resource(resource& value) noexcept {
        const u32 used = __atomic_load_n(&value.used_pages, __ATOMIC_ACQUIRE);
        if (used != 0U)
            __atomic_fetch_sub(&value.used_pages, 1U, __ATOMIC_ACQ_REL);
    }

    [[nodiscard]] inline error_t resolve_resource(task::task& owner, capability_id_t selector,
                                                  resource*& result) noexcept {
        result = nullptr;
        object::header_t* header = nullptr;
        const error_t lookup =
            capability::lookup(owner.cspace, selector, object::type_t::memory_resource,
                               capability::right_t::write, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<resource*>(header);
        if (!same_reference(result->owner_task, object::reference(owner.object)))
            return error_t::denied;
        return error_t::success;
    }

    [[nodiscard]] inline error_t destroy_resource(task::task& owner,
                                                  capability_id_t selector) noexcept;

    [[nodiscard]] inline error_t delegate_resource(task::task& source_owner, resource& parent,
                                                   task::task& target_owner,
                                                   capability_id_t destination,
                                                   u32 quota_pages) noexcept {
        if (!source_owner.root &&
            !same_reference(parent.owner_task, object::reference(source_owner.object)))
            return error_t::denied;
        u32 delegated = __atomic_load_n(&parent.delegated_pages, __ATOMIC_ACQUIRE);
        for (;;) {
            const u32 used = __atomic_load_n(&parent.used_pages, __ATOMIC_ACQUIRE);
            if (quota_pages == 0U || used + delegated + quota_pages > parent.quota_pages)
                return error_t::no_memory;
            if (__atomic_compare_exchange_n(&parent.delegated_pages, &delegated,
                                            delegated + quota_pages, false, __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE))
                break;
        }
        error_t result = create_resource(target_owner, destination, quota_pages,
                                         object::reference(parent.object), false);
        const bool resource_created = result == error_t::success;
        if (resource_created) {
            object::header_t* child_header = nullptr;
            result = capability::lookup(target_owner.cspace, destination,
                                        object::type_t::memory_resource,
                                        capability::right_t::control, child_header);
            if (result == error_t::success) {
                lock_allocator();
                result = carve_resource_extents(parent, *reinterpret_cast<resource*>(child_header),
                                                quota_pages);
                unlock_allocator();
            }
            if (result != error_t::success)
                (void)destroy_resource(target_owner, destination);
        }
        if (result != error_t::success && !resource_created)
            __atomic_fetch_sub(&parent.delegated_pages, quota_pages, __ATOMIC_ACQ_REL);
        return result;
    }

    [[nodiscard]] inline error_t destroy_resource(task::task& owner,
                                                  capability_id_t selector) noexcept {
        object::header_t* header = nullptr;
        error_t result = capability::lookup(owner.cspace, selector, object::type_t::memory_resource,
                                            capability::right_t::control, header);
        if (result != error_t::success)
            return result;
        auto& target = *reinterpret_cast<resource*>(header);
        if (target.root || !same_reference(target.owner_task, object::reference(owner.object)))
            return error_t::denied;
        if (__atomic_load_n(&target.used_pages, __ATOMIC_ACQUIRE) != 0U ||
            __atomic_load_n(&target.delegated_pages, __ATOMIC_ACQUIRE) != 0U)
            return error_t::busy;
        if (valid_reference(target.parent)) {
            auto* parent = reinterpret_cast<resource*>(object::resolve(target.parent));
            if (parent != nullptr) {
                lock_allocator();
                while (target.extent_head != invalid_extent_index) {
                    const u32 node = target.extent_head;
                    target.extent_head = extent_nodes[node].next;
                    --target.extent_count;
                    extent_nodes[node].next = invalid_extent_index;
                    insert_extent_node_sorted(*parent, node);
                }
                unlock_allocator();
                __atomic_fetch_sub(&parent->delegated_pages, target.quota_pages, __ATOMIC_ACQ_REL);
            }
        }
        const object::reference_t reference = object::reference(target.object);
        capability::revoke_reference(reference);
        (void)capability::delete_capability(owner.cspace, selector);
        result = object::unregister_object(reference);
        if (result != error_t::success)
            return result;
        clear_resource(target);
        return error_t::success;
    }

    inline error_t initialize_objects() noexcept {
        error_t result = initialize_physical_allocator();
        if (result != error_t::success)
            return result;
        for (u32 index = 0U; index < bootstrap_frame_count; ++index) {
            frames[index].in_use = true;
            result = object::register_object(
                frames[index].object,
                static_cast<object_id_t>(object::bootstrap_id::frame_base + index),
                object::type_t::frame);
            if (result != error_t::success)
                return result;
            result = assign_frame(frames[index], 0U);
            if (result != error_t::success)
                return result;
        }
        for (u32 index = 0U; index < bootstrap_page_table_count; ++index) {
            page_tables[index].level = 3U;
            page_tables[index].in_use = true;
            result = object::register_object(
                page_tables[index].object,
                static_cast<object_id_t>(object::bootstrap_id::page_table_base + index),
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
            result = assign_frame(*target, owner.address_space_id, object::reference(owner.object));
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

    [[nodiscard]] inline error_t create_frame_from_resource(task::task& owner, resource& authority,
                                                            capability_id_t destination) noexcept {
        if (destination >= capability::cspace_slot_count)
            return error_t::invalid_argument;
        error_t result = charge_resource(authority);
        if (result != error_t::success)
            return result;
        result = charge_page(owner);
        if (result != error_t::success) {
            uncharge_resource(authority);
            return result;
        }
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
            uncharge_resource(authority);
            return error_t::no_memory;
        }
        result = object::register_dynamic_object(target->object, object::type_t::frame);
        if (result == error_t::success)
            result = assign_frame(*target, owner.address_space_id, object::reference(owner.object),
                                  &authority);
        if (result == error_t::success) {
            target->resource_authority = object::reference(authority.object);
            const capability::rights_t rights{static_cast<u32>(capability::right_t::read) |
                                              static_cast<u32>(capability::right_t::write) |
                                              static_cast<u32>(capability::right_t::grant) |
                                              static_cast<u32>(capability::right_t::control)};
            result = capability::install(owner.cspace, destination,
                                         object::reference(target->object), rights);
        }
        if (result != error_t::success) {
            if (target->allocated)
                (void)release_frame(*target);
            if (target->object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target->object));
            clear_frame(*target);
            uncharge_page(owner);
            uncharge_resource(authority);
        }
        return result;
    }

    [[nodiscard]] inline error_t create_device_frame(task::task& owner, capability_id_t destination,
                                                     paddr_t address) noexcept {
        if (!owner.root || destination >= capability::cspace_slot_count ||
            (address & (page_size - 1U)) != 0U || !platform::memory::valid_device_page(address))
            return error_t::denied;
        frame* target = nullptr;
        for (u32 index = bootstrap_frame_count; index < frame_count; ++index) {
            bool expected = false;
            if (__atomic_compare_exchange_n(&frames[index].in_use, &expected, true, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                target = &frames[index];
                break;
            }
        }
        if (target == nullptr)
            return error_t::no_memory;
        error_t result = object::register_dynamic_object(target->object, object::type_t::frame);
        if (result == error_t::success) {
            target->physical_address = address;
            target->owner = owner.address_space_id;
            target->owner_task = object::reference(owner.object);
            target->mapping_count = 0U;
            target->allocated = true;
            target->device = true;
            for (auto& mapping : target->mappings)
                mapping = {};
            const capability::rights_t rights{static_cast<u32>(capability::right_t::read) |
                                              static_cast<u32>(capability::right_t::write) |
                                              static_cast<u32>(capability::right_t::grant) |
                                              static_cast<u32>(capability::right_t::control)};
            result = capability::install(owner.cspace, destination,
                                         object::reference(target->object), rights);
        }
        if (result != error_t::success) {
            if (target->object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target->object));
            target->object = {};
            target->physical_address = 0U;
            target->owner = 0U;
            target->owner_task = {};
            target->mapping_count = 0U;
            target->allocated = false;
            target->device = false;
            for (auto& mapping : target->mappings)
                mapping = {};
            __atomic_store_n(&target->in_use, false, __ATOMIC_RELEASE);
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
        const bool was_device = target.device;
        resource* charged_resource = nullptr;
        if (valid_reference(target.resource_authority))
            charged_resource =
                reinterpret_cast<resource*>(object::resolve(target.resource_authority));
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
        if (!was_device)
            uncharge_page(owner);
        if (charged_resource != nullptr)
            uncharge_resource(*charged_resource);
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
            target->owner_task = object::reference(owner.object);
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

    [[nodiscard]] inline error_t create_page_table_from_resource(task::task& owner,
                                                                 resource& authority,
                                                                 capability_id_t destination,
                                                                 u8 level) noexcept {
        if (destination >= capability::cspace_slot_count || level > 3U)
            return error_t::invalid_argument;
        error_t result = charge_resource(authority);
        if (result != error_t::success)
            return result;
        result = charge_page(owner);
        if (result != error_t::success) {
            uncharge_resource(authority);
            return result;
        }
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
            uncharge_resource(authority);
            return error_t::no_memory;
        }
        result = object::register_dynamic_object(target->object, object::type_t::page_table);
        if (result == error_t::success)
            result = allocate_resource_page(authority, target->physical_address);
        if (result == error_t::success) {
            target->owner = owner.address_space_id;
            target->owner_task = object::reference(owner.object);
            target->resource_authority = object::reference(authority.object);
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
            uncharge_resource(authority);
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
        resource* charged_resource = nullptr;
        if (valid_reference(target.resource_authority))
            charged_resource =
                reinterpret_cast<resource*>(object::resolve(target.resource_authority));
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
        if (charged_resource != nullptr)
            uncharge_resource(*charged_resource);
        return error_t::success;
    }

    [[nodiscard]] inline error_t map(space::address_space& target, frame& source, vaddr_t address,
                                     permission permissions,
                                     capability::derivation_id_t frame_authority,
                                     capability::derivation_id_t space_authority,
                                     mapping_attributes attributes = {}) noexcept {
        if (!valid_permission(permissions) || !source.allocated ||
            !valid_attributes(attributes, permissions, source.device))
            return error_t::denied;
        const object::reference_t space_reference = object::reference(target.object);
        if (!valid_reference(space_reference))
            return error_t::invalid_argument;

        lock_mappings();
        for (const auto& mapping : source.mappings) {
            if (mapping.valid && same_reference(mapping.address_space, space_reference) &&
                mapping.address == address) {
                unlock_mappings();
                return error_t::busy;
            }
        }
        mapping_record* free_record = nullptr;
        for (auto& mapping : source.mappings) {
            if (!mapping.valid) {
                free_record = &mapping;
                break;
            }
        }
        if (free_record == nullptr) {
            unlock_mappings();
            return error_t::no_memory;
        }

        const error_t result = target.map_page(
            address, reinterpret_cast<void*>(static_cast<uintptr_t>(source.physical_address)),
            writable(permissions), executable(permissions), source.device,
            attributes.share == shareability::inner_shareable);
        if (result != error_t::success) {
            unlock_mappings();
            return result;
        }

        u32 generation = next_mapping_generation++;
        if (generation == 0U)
            generation = next_mapping_generation++;
        *free_record = {space_reference, address,         permissions, attributes,
                        frame_authority, space_authority, generation,  true};
        ++source.mapping_count;
        unlock_mappings();
        return error_t::success;
    }

    [[nodiscard]] inline error_t unmap(space::address_space& target, frame& source,
                                       vaddr_t address = 0U) noexcept {
        const object::reference_t space_reference = object::reference(target.object);
        lock_mappings();
        for (auto& mapping : source.mappings) {
            if (!mapping.valid || !same_reference(mapping.address_space, space_reference) ||
                (address != 0U && mapping.address != address))
                continue;
            const error_t result = target.unmap_page(mapping.address);
            if (result != error_t::success) {
                unlock_mappings();
                return result;
            }
            mapping = {};
            if (source.mapping_count != 0U)
                --source.mapping_count;
            unlock_mappings();
            return error_t::success;
        }
        unlock_mappings();
        return error_t::not_found;
    }

    inline u32 unmap_authority(capability::derivation_id_t authority,
                               bool descendants_only) noexcept {
        if (authority == 0U)
            return 0U;
        u32 removed = 0U;
        lock_mappings();
        for (auto& source : frames) {
            if (!source.allocated || source.mapping_count == 0U)
                continue;
            for (auto& mapping : source.mappings) {
                if (!mapping.valid)
                    continue;
                const auto matches =
                    [authority, descendants_only](capability::derivation_id_t candidate) noexcept {
                        if (!descendants_only && candidate == authority)
                            return true;
                        return capability::descendant_of(candidate, authority);
                    };
                if (!matches(mapping.frame_authority) && !matches(mapping.space_authority))
                    continue;
                object::header_t* header = object::resolve(mapping.address_space);
                if (header != nullptr && header->type == object::type_t::address_space) {
                    auto& target = *reinterpret_cast<space::address_space*>(header);
                    const error_t result = target.unmap_page(mapping.address);
                    if (result != error_t::success && result != error_t::not_found)
                        continue;
                }
                mapping = {};
                if (source.mapping_count != 0U)
                    --source.mapping_count;
                ++removed;
            }
        }
        unlock_mappings();
        return removed;
    }

    [[nodiscard]] inline error_t unmap_all(frame& source) noexcept {
        if (!source.allocated)
            return error_t::not_found;
        error_t first_error = error_t::success;
        lock_mappings();
        for (auto& mapping : source.mappings) {
            if (!mapping.valid)
                continue;
            object::header_t* header = object::resolve(mapping.address_space);
            if (header == nullptr || header->type != object::type_t::address_space) {
                mapping = {};
                if (source.mapping_count != 0U)
                    --source.mapping_count;
                continue;
            }
            auto& target = *reinterpret_cast<space::address_space*>(header);
            const error_t result = target.unmap_page(mapping.address);
            if (result != error_t::success && result != error_t::not_found &&
                first_error == error_t::success)
                first_error = result;
            if (result == error_t::success || result == error_t::not_found) {
                mapping = {};
                if (source.mapping_count != 0U)
                    --source.mapping_count;
            }
        }
        unlock_mappings();
        return first_error;
    }

    inline void unmap_all(space::address_space& target) noexcept {
        const object::reference_t space_reference = object::reference(target.object);
        lock_mappings();
        for (auto& source : frames) {
            if (!source.allocated || source.mapping_count == 0U)
                continue;
            for (auto& mapping : source.mappings) {
                if (!mapping.valid || !same_reference(mapping.address_space, space_reference))
                    continue;
                const error_t result = target.unmap_page(mapping.address);
                if (result != error_t::success && result != error_t::not_found)
                    continue;
                mapping = {};
                if (source.mapping_count != 0U)
                    --source.mapping_count;
            }
        }
        unlock_mappings();
    }

    [[nodiscard]] inline u32 allocated_page_count() noexcept {
        return managed_pages - __atomic_load_n(&free_pages, __ATOMIC_ACQUIRE);
    }

} // namespace sys::kernel::memory
