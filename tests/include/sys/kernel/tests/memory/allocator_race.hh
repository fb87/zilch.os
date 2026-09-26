#pragma once

#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/printk.hh>
#include <sys/platform/interrupt.hh>

namespace sys::kernel::tests::allocator_race
{
    /*
     * Multi-CPU allocator pressure (TST-023).
     *
     * `allocator_exhaustion` drives a 512-page burst from one CPU and proves
     * the single-threaded contract: aligned, distinct, accounted, balanced,
     * and failing closed with `no_memory` at empty. The property it cannot
     * reach is the one that matters most and is checked least -- that two
     * CPUs are never handed the same page.
     *
     * Every CPU here contends for the same page deliberately. Each releases
     * its page immediately, so the bitmap stays nearly empty and
     * `allocate_physical_page`, which rescans from page zero on every call
     * (the O(n^2) property recorded in 0167), keeps returning the same few
     * low addresses to all three workers at once. That is the worst case for
     * mutual exclusion rather than an accident of the test.
     *
     * Detection is direct rather than statistical. A worker stamps its own
     * page with a value unique to it, waits, and reads it back: if another
     * CPU was handed the same page, the stamp it finds is not the one it
     * wrote. An allocator that double-hands a page fails this immediately,
     * where a test that only compared returned addresses after the fact
     * could miss it entirely -- both CPUs release, the counts balance, and
     * nothing looks wrong.
     */
    inline constexpr u32 race_cpu_count = 4U;
    inline constexpr u32 iterations_per_cpu = 128U;

    inline volatile bool active{};
    inline volatile u32 claimed_mask{};
    inline volatile u32 done_mask{};
    inline volatile u64 allocations[race_cpu_count]{};
    inline volatile u64 collisions[race_cpu_count]{};
    inline volatile u64 exhaustions[race_cpu_count]{};
    inline volatile u64 misaligned[race_cpu_count]{};

    inline void service_job(cpu_id_t cpu) noexcept {
        if (!__atomic_load_n(&active, __ATOMIC_ACQUIRE) || cpu >= race_cpu_count)
            return;
        const u32 bit = 1U << cpu;
        if ((__atomic_fetch_or(&claimed_mask, bit, __ATOMIC_ACQ_REL) & bit) != 0U)
            return;

        u64 local_allocations = 0U;
        u64 local_collisions = 0U;
        u64 local_exhaustions = 0U;
        u64 local_misaligned = 0U;

        for (u32 index = 0U; index < iterations_per_cpu; ++index) {
            paddr_t page{};
            if (memory::allocate_physical_page(page) != error_t::success) {
                ++local_exhaustions;
                continue;
            }
            if ((page & (memory::page_size - 1U)) != 0U)
                ++local_misaligned;

            /*
             * The page is free memory the allocator just handed over, and
             * RAM is identity-mapped in the kernel, so it can be written
             * directly. The stamp carries the CPU as well as the iteration
             * so a collision cannot be masked by two workers happening to
             * be on the same iteration count.
             */
            auto* const stamp = reinterpret_cast<volatile u64*>(static_cast<uintptr_t>(page));
            const u64 mark = (static_cast<u64>(cpu) << 32U) | index;
            *stamp = mark;

            // Long enough for another CPU holding the same page to overwrite
            // the stamp, short enough not to monopolise the interrupt.
            for (u32 wait = 0U; wait < 64U; ++wait)
                arch::cpu::relax();

            if (*stamp != mark)
                ++local_collisions;
            ++local_allocations;

            if (memory::release_physical_page(page) != error_t::success)
                ++local_collisions; // releasing what we were given must work
        }

        __atomic_store_n(&allocations[cpu], local_allocations, __ATOMIC_RELAXED);
        __atomic_store_n(&collisions[cpu], local_collisions, __ATOMIC_RELAXED);
        __atomic_store_n(&exhaustions[cpu], local_exhaustions, __ATOMIC_RELAXED);
        __atomic_store_n(&misaligned[cpu], local_misaligned, __ATOMIC_RELAXED);
        __atomic_fetch_or(&done_mask, bit, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline error_t run() noexcept
    {
        const u32 free_before = memory::free_pages;

        __atomic_store_n(&claimed_mask, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&done_mask, 0U, __ATOMIC_RELEASE);
        for (u32 cpu = 0U; cpu < race_cpu_count; ++cpu) {
            __atomic_store_n(&allocations[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&collisions[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&exhaustions[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&misaligned[cpu], 0U, __ATOMIC_RELAXED);
        }
        __atomic_store_n(&active, true, __ATOMIC_RELEASE);

        for (cpu_id_t cpu = 1U; cpu < race_cpu_count; ++cpu)
            platform::interrupt::send_ipi(cpu, platform::interrupt::reschedule_ipi);

        /*
         * CPU 0 joins the contention rather than only waiting, so all four
         * compete for the same low pages -- hence all four bits in the
         * completion mask, not the three it woke.
         */
        constexpr u32 worker_mask = (1U << race_cpu_count) - 1U;
        service_job(0U);

        u32 spins = 0U;
        while (__atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) != worker_mask &&
               spins++ < 2000000U)
            arch::cpu::relax();
        __atomic_store_n(&active, false, __ATOMIC_RELEASE);

        const bool completed = __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) == worker_mask;

        u64 total_allocations = __atomic_load_n(&allocations[0], __ATOMIC_RELAXED);
        u64 total_collisions = __atomic_load_n(&collisions[0], __ATOMIC_RELAXED);
        u64 total_exhaustions = __atomic_load_n(&exhaustions[0], __ATOMIC_RELAXED);
        u64 total_misaligned = __atomic_load_n(&misaligned[0], __ATOMIC_RELAXED);
        for (u32 cpu = 1U; cpu < race_cpu_count; ++cpu) {
            total_allocations += __atomic_load_n(&allocations[cpu], __ATOMIC_RELAXED);
            total_collisions += __atomic_load_n(&collisions[cpu], __ATOMIC_RELAXED);
            total_exhaustions += __atomic_load_n(&exhaustions[cpu], __ATOMIC_RELAXED);
            total_misaligned += __atomic_load_n(&misaligned[cpu], __ATOMIC_RELAXED);
        }

        /*
         * Every page taken must come back. A counter that balances while
         * pages were double-handed would still be wrong, which is why the
         * stamp check above exists -- but a counter that does NOT balance
         * is a leak or a double-free, and this is where that shows.
         */
        const bool balanced = memory::free_pages == free_before;

        if (!completed || total_collisions != 0U || total_misaligned != 0U || !balanced ||
            total_allocations == 0U) {
            pr_err("[TEST] name=allocator_multicpu_race result=FAIL completed=%u allocations=%llu "
                   "collisions=%llu exhaustions=%llu misaligned=%llu free_before=%u free_after=%u "
                   "done_mask=%x\n",
                   completed ? 1U : 0U, static_cast<unsigned long long>(total_allocations),
                   static_cast<unsigned long long>(total_collisions),
                   static_cast<unsigned long long>(total_exhaustions),
                   static_cast<unsigned long long>(total_misaligned), free_before,
                   memory::free_pages, __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE));
            return error_t::invalid_argument;
        }

        pr_info("[TEST] name=allocator_multicpu_race result=PASS cpus=%u allocations=%llu "
                "collisions=0 misaligned=0 exhaustions=%llu balanced=1\n",
                race_cpu_count, static_cast<unsigned long long>(total_allocations),
                static_cast<unsigned long long>(total_exhaustions));
        return error_t::success;
    }
} // namespace sys::kernel::tests::allocator_race
