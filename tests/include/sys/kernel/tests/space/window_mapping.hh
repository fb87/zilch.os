#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

/*
 * Covers the parts of the widened user address space that no production
 * path reaches yet.
 *
 * A process used to hold exactly one L3 table covering one 2 MiB block, and
 * map_page() refused every address outside the image window, so "map
 * something into a second block" was not an expressible operation and the
 * on-demand table allocation added for it would otherwise ship with nothing
 * executing it. This maps into a high block, proves the mapping is real by
 * round-tripping a value through it, and proves the table is reclaimed
 * rather than leaked when the space is released.
 *
 * It also pins the stack guard, which is the invariant this rework nearly
 * lost: userspace had been mapping scratch pages onto the guard address for
 * as long as the guard existed, and nothing noticed because a one-page
 * stack could not overflow into it.
 */
namespace sys::kernel::tests::space
{
    [[nodiscard]] inline error_t run_window_mapping(thread::thread& root_thread) noexcept {
        auto& native = root_thread.address_space.native;

        /* A block well past block 0, so this cannot pass by accident on the
         * statically embedded table. */
        constexpr usize_t probe_block = 3U;
        static_assert(probe_block < arch::space::user_block_count);
        constexpr vaddr_t probe =
            arch::space::user_window_base + probe_block * arch::space::user_block_size;

        if (arch::space::page_descriptor(native, probe) != 0U)
            return error_t::invalid_argument;

        paddr_t backing = 0U;
        if (memory::allocate_physical_page(backing) != error_t::success || backing == 0U)
            return error_t::no_memory;

        const error_t mapped = arch::space::map_page(
            native, probe, reinterpret_cast<void*>(static_cast<uintptr_t>(backing)), true, false,
            false, true, &memory::allocate_physical_page);
        if (mapped != error_t::success) {
            (void)memory::release_physical_page(backing);
            return mapped;
        }

        const auto fail = [&](error_t reason) noexcept -> error_t {
            (void)arch::space::unmap_page(native, probe);
            (void)memory::release_physical_page(backing);
            return reason;
        };

        /* The table must now exist, and must be a real one rather than
         * block 0's aliased back. */
        if (native.l3_tables[probe_block] == nullptr ||
            native.l3_backing[probe_block] == 0U ||
            native.l3_tables[probe_block] == &native.l3)
            return fail(error_t::invalid_argument);

        /* Bits [47:12] only: the descriptor also carries PXN/UXN up at 53
         * and 54, so a plain ~0xfff mask never matches a bare address. */
        constexpr u64 address_field = 0x0000fffffffff000ULL;
        const u64 descriptor = arch::space::page_descriptor(native, probe);
        if ((descriptor & arch::memory::descriptor_valid) == 0U ||
            (descriptor & address_field) != (backing & address_field))
            return fail(error_t::invalid_argument);

        /* Round-trip through the physical page the descriptor names, which
         * is what makes this a mapping test rather than a bookkeeping one.
         * The kernel reaches it identity-mapped; EL0 reachability is the
         * separate concern user_access::valid_range() covers. */
        auto* const cell = reinterpret_cast<volatile u64*>(static_cast<uintptr_t>(backing));
        *cell = 0x5ec0ffeed15ea5edULL;
        arch::cpu::store_barrier();
        if (*cell != 0x5ec0ffeed15ea5edULL)
            return fail(error_t::invalid_argument);

        /* Re-mapping an occupied address is busy, not a silent overwrite. */
        if (arch::space::map_page(native, probe,
                                  reinterpret_cast<void*>(static_cast<uintptr_t>(backing)), true,
                                  false, false, true,
                                  &memory::allocate_physical_page) != error_t::busy)
            return fail(error_t::invalid_argument);

        /* The guard page is refused by position, whatever its state. */
        if (arch::space::map_page(native, arch::space::user_stack_base - arch::memory::page_size,
                                  reinterpret_cast<void*>(static_cast<uintptr_t>(backing)), true,
                                  false, false, true,
                                  &memory::allocate_physical_page) != error_t::invalid_argument)
            return fail(error_t::invalid_argument);

        /* So is anything past the window's end. */
        if (arch::space::map_page(native, arch::space::user_window_end,
                                  reinterpret_cast<void*>(static_cast<uintptr_t>(backing)), true,
                                  false, false, true,
                                  &memory::allocate_physical_page) != error_t::invalid_argument)
            return fail(error_t::invalid_argument);

        if (arch::space::unmap_page(native, probe) != error_t::success ||
            arch::space::page_descriptor(native, probe) != 0U ||
            arch::space::unmap_page(native, probe) != error_t::not_found)
            return fail(error_t::invalid_argument);

        /*
         * Release the on-demand table the way teardown does, and confirm the
         * page actually comes back. Leaking here would be invisible in
         * ordinary operation until the physical pool ran dry, which is the
         * shape of bug USR-034 already hit once from the frame side.
         */
        const u32 free_before = __atomic_load_n(&memory::free_pages, __ATOMIC_ACQUIRE);
        arch::space::release_dynamic_tables(native, &memory::release_physical_page);
        if (native.l3_tables[probe_block] != nullptr ||
            native.l3_backing[probe_block] != 0U ||
            __atomic_load_n(&memory::free_pages, __ATOMIC_ACQUIRE) != free_before + 1U)
            return fail(error_t::invalid_argument);

        if (memory::release_physical_page(backing) != error_t::success)
            return error_t::invalid_argument;

        pr_info("[TEST] name=user_window_dynamic_mapping result=PASS blocks=%u stack_pages=%u\n",
                static_cast<unsigned int>(arch::space::user_block_count),
                static_cast<unsigned int>(arch::space::user_stack_pages));
        return error_t::success;
    }
} // namespace sys::kernel::tests::space
