#pragma once

#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>

namespace sys::kernel::tests::revoke_race
{
    /*
     * Concurrent map/unmap against revocation-driven teardown (TST-019).
     *
     * The deterministic revoke-driven unmapping test proves the single-CPU
     * ordering. It cannot say anything about the case the mapping lock
     * actually exists for: one CPU tearing every mapping off a frame while
     * others are adding and removing their own. That window is narrow, it is
     * reached only under real parallelism, and the consequences of losing it
     * are not a clean failure -- a mapping record leaked or double-freed
     * leaves `mapping_count` disagreeing with `mappings[]`, which is a
     * corruption the system carries silently until something much later
     * trips over it.
     *
     * Three CPUs map and unmap the same frame at their own addresses while
     * CPU 0 repeatedly calls unmap_all() on it. unmap_all() is what
     * capability revocation drives, so this is the revoke path rather than a
     * stand-in for it, and every unmap issues a TLB shootdown, so the
     * shootdown races come along for free rather than needing a separate
     * harness.
     *
     * Dispatch reuses the reschedule-IPI work lane the hypervisor SMP test
     * already established. The job runs in interrupt context on CPUs that
     * are idle during the bootstrap self-test, so it interrupts no lock
     * holder; the completion wait is bounded, so a CPU that never reports
     * fails the test instead of hanging the boot.
     */
    inline constexpr u32 race_cpu_count = 4U;
    /*
     * Kept deliberately small. The job body runs in interrupt context, so
     * every iteration is time the timer on that CPU is not being serviced,
     * and the certification latency bounds track a RUNNING maximum -- a long
     * loop here raises a number checked seconds later, in a test that has
     * nothing to do with this one. 128 still interleaves hundreds of
     * teardowns against the map/unmap traffic, which is what the race needs;
     * more iterations buy repetition, not coverage.
     */
    inline constexpr u32 iterations_per_cpu = 128U;

    /*
     * Inside the 2 MiB block root's own mappings already live in, but well
     * above the addresses it uses (its highest is around 0x20052000).
     *
     * Not an arbitrary choice, and not the obvious one: a clearly-unrelated
     * base like 0x30000000 fails every map with invalid_argument, because
     * the arch layer will allocate a missing L3 table for a block but not
     * the L2 above it, so a VA in a region the space has no coverage for is
     * simply unmappable. Staying in a block that already has a table also
     * means the race measures the mapping database rather than page-table
     * allocation, which is a different test.
     */
    inline constexpr vaddr_t race_base = 0x20100000ULL;

    inline memory::frame contended{};
    inline volatile bool active{};
    inline volatile u32 claimed_mask{};
    inline volatile u32 done_mask{};
    inline volatile u64 operations[race_cpu_count]{};
    inline volatile u64 anomalies[race_cpu_count]{};
    inline volatile s32 first_map_error[race_cpu_count]{};
    inline volatile s32 first_unmap_error[race_cpu_count]{};

    /*
     * Results a racing caller is entitled to see. Anything else means the
     * operation neither succeeded nor failed for a reason the contract
     * describes, which is the interesting kind of wrong: `busy` is another
     * CPU holding the same address, `not_found` is unmap_all() having got
     * there first, `denied` is the frame being released mid-flight.
     */
    [[nodiscard]] inline bool tolerated(error_t value) noexcept {
        return value == error_t::success || value == error_t::busy ||
               value == error_t::not_found || value == error_t::denied;
    }

    inline void service_job(cpu_id_t cpu) noexcept {
        if (!__atomic_load_n(&active, __ATOMIC_ACQUIRE) || cpu >= race_cpu_count)
            return;
        const u32 bit = 1U << cpu;
        if ((__atomic_fetch_or(&claimed_mask, bit, __ATOMIC_ACQ_REL) & bit) != 0U)
            return;

        auto& space = thread::user_threads[0].address_space;
        const vaddr_t address = race_base + static_cast<vaddr_t>(cpu) * memory::page_size;
        u64 local_anomalies = 0U;

        constexpr auto read_write = static_cast<memory::permission>(
            static_cast<u8>(memory::permission::read) | static_cast<u8>(memory::permission::write));

        for (u32 index = 0U; index < iterations_per_cpu; ++index) {
            const error_t mapped = memory::map(space, contended, address, read_write, 0U, 0U);
            if (!tolerated(mapped)) {
                ++local_anomalies;
                if (first_map_error[cpu] == 0)
                    first_map_error[cpu] = static_cast<s32>(mapped);
            }
            const error_t unmapped = memory::unmap(space, contended, address);
            if (!tolerated(unmapped)) {
                ++local_anomalies;
                if (first_unmap_error[cpu] == 0)
                    first_unmap_error[cpu] = static_cast<s32>(unmapped);
            }
        }

        __atomic_store_n(&operations[cpu], iterations_per_cpu * 2U, __ATOMIC_RELAXED);
        __atomic_store_n(&anomalies[cpu], local_anomalies, __ATOMIC_RELAXED);
        __atomic_fetch_or(&done_mask, bit, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline error_t run() noexcept
    {
        const u32 free_before = memory::free_pages;

        if (memory::assign_frame(contended, thread::user_threads[0].owner->address_space_id) !=
            error_t::success)
            return error_t::invalid_argument;

        __atomic_store_n(&claimed_mask, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&done_mask, 0U, __ATOMIC_RELEASE);
        for (u32 cpu = 0U; cpu < race_cpu_count; ++cpu) {
            __atomic_store_n(&operations[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&anomalies[cpu], 0U, __ATOMIC_RELAXED);
        }
        __atomic_store_n(&active, true, __ATOMIC_RELEASE);

        for (cpu_id_t cpu = 1U; cpu < race_cpu_count; ++cpu)
            platform::interrupt::send_ipi(cpu, platform::interrupt::reschedule_ipi);

        /*
         * CPU 0 is the revoker, and it runs the same work the others do
         * rather than only waiting: a teardown that never overlaps a live
         * map() would exercise nothing. unmap_all() is the operation
         * capability revocation performs on a frame.
         */
        constexpr u32 all_cpus = (1U << race_cpu_count) - 1U;
        u64 teardowns = 0U;
        u64 revoker_anomalies = 0U;
        u32 spins = 0U;
        while (__atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) != (all_cpus & ~1U) &&
               spins++ < 2000000U) {
            if (!tolerated(memory::unmap_all(contended)))
                ++revoker_anomalies;
            ++teardowns;
        }
        __atomic_store_n(&active, false, __ATOMIC_RELEASE);

        const bool completed = __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) == (all_cpus & ~1U);

        // Whatever the outcome, leave nothing mapped behind.
        (void)memory::unmap_all(contended);

        u64 total_operations = 0U;
        u64 total_anomalies = revoker_anomalies;
        for (u32 cpu = 1U; cpu < race_cpu_count; ++cpu) {
            total_operations += __atomic_load_n(&operations[cpu], __ATOMIC_RELAXED);
            total_anomalies += __atomic_load_n(&anomalies[cpu], __ATOMIC_RELAXED);
        }

        /*
         * The accounting check is the real payload. A race that loses a
         * mapping record does not fail any individual call -- it leaves
         * mapping_count disagreeing with the records, and the frame can then
         * never be fully released. Counting the valid records by hand and
         * comparing is the only way to see it.
         */
        u32 live_records = 0U;
        for (const auto& mapping : contended.mappings) {
            if (mapping.valid)
                ++live_records;
        }
        const u32 contended_mapping_count = contended.mapping_count;
        const bool accounting_consistent = live_records == 0U && contended_mapping_count == 0U;
        const bool database_valid = memory::mapping_database_valid();

        if (memory::release_frame(contended) != error_t::success)
            return error_t::invalid_argument;
        // Field by field rather than `contended = {}`: the aggregate
        // assignment emits a memcpy, which does not exist in a freestanding
        // build.
        contended.physical_address = 0U;
        contended.owner = 0U;
        contended.owner_task = {};
        contended.mapping_count = 0U;
        contended.allocated = false;
        contended.device = false;
        __atomic_store_n(&contended.in_use, false, __ATOMIC_RELEASE);

        if (!completed || total_anomalies != 0U || !accounting_consistent || !database_valid ||
            memory::free_pages != free_before) {
            pr_err("[TEST] name=concurrent_revoke_map_unmap result=FAIL completed=%u "
                   "operations=%llu teardowns=%llu anomalies=%llu live_records=%u "
                   "mapping_count=%u database=%u free_before=%u free_after=%u done_mask=%x "
                   "map_err=%d,%d,%d unmap_err=%d,%d,%d\n",
                   completed ? 1U : 0U, static_cast<unsigned long long>(total_operations),
                   static_cast<unsigned long long>(teardowns),
                   static_cast<unsigned long long>(total_anomalies), live_records,
                   contended_mapping_count, database_valid ? 1U : 0U, free_before,
                   memory::free_pages, __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE),
                   first_map_error[1], first_map_error[2], first_map_error[3],
                   first_unmap_error[1], first_unmap_error[2], first_unmap_error[3]);
            return error_t::invalid_argument;
        }

        pr_info("[TEST] name=concurrent_revoke_map_unmap result=PASS cpus=%u operations=%llu "
                "teardowns=%llu anomalies=0 mapping_count=0 database=valid pages_balanced=1\n",
                race_cpu_count - 1U, static_cast<unsigned long long>(total_operations),
                static_cast<unsigned long long>(teardowns));
        return error_t::success;
    }
} // namespace sys::kernel::tests::revoke_race
