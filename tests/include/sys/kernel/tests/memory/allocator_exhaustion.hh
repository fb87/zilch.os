#pragma once

#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::allocator_exhaustion
{
    /*
     * The physical allocator's behaviour at and around empty (TST-023).
     *
     * Two halves, because one of them cannot be done the obvious way.
     *
     * Real exhaustion by repeated allocation is not viable here:
     * allocate_physical_page() rescans every region from page zero on every
     * call, so taking n pages costs O(n^2), and this machine manages around
     * 64,000 of them. That is a finding rather than an excuse -- the
     * allocator is linear per allocation and nothing currently allocates
     * often enough to notice -- but it means a genuine
     * allocate-until-empty loop would run for billions of iterations inside
     * a boot-time self-test.
     *
     * So the empty case is reached by snapshotting the allocator, marking
     * it full, checking the contract, and restoring. That is honest about
     * what it proves: the behaviour AT empty, not the journey there. The
     * journey is covered separately by a bounded burst that exercises real
     * allocation, accounting and balanced release.
     *
     * Safe to drive from the bootstrap self-test because it runs on CPU 0
     * during boot with secondaries idle, and because the snapshot is
     * restored before anything else can allocate.
     */
    [[nodiscard]] inline error_t run() noexcept
    {
        using namespace sys::kernel::memory;

        constexpr u32 burst = 512U;
        static paddr_t taken[burst]{};

        const u32 free_before = free_pages;
        if (free_before <= burst)
            return error_t::invalid_argument; // nothing to prove on a tiny machine

        /*
         * A bounded real burst. Every allocation must succeed, be page
         * aligned, be distinct from the previous one, and move the counter
         * by exactly one -- an allocator that handed out the same page
         * twice, or forgot to account for one, would still "work" until
         * two owners collided.
         */
        for (u32 index = 0U; index < burst; ++index) {
            if (allocate_physical_page(taken[index]) != error_t::success)
                return error_t::invalid_argument;
            if (taken[index] == 0U || (taken[index] & (page_size - 1U)) != 0U)
                return error_t::invalid_argument;
            if (index != 0U && taken[index] == taken[index - 1U])
                return error_t::invalid_argument;
            if (free_pages != free_before - index - 1U)
                return error_t::invalid_argument;
        }

        for (u32 index = 0U; index < burst; ++index) {
            if (release_physical_page(taken[index]) != error_t::success)
                return error_t::invalid_argument;
        }
        if (free_pages != free_before)
            return error_t::invalid_argument; // release must balance exactly

        /*
         * Now the empty case. Snapshot, fill, check, restore. The snapshot
         * is a plain loop rather than a copy: a freestanding kernel links
         * no memcpy, and a struct assignment here would emit one.
         */
        static u64 snapshot[bitmap_words]{};
        for (u32 word = 0U; word < bitmap_words; ++word)
            snapshot[word] = allocation_bitmap[word];
        const u32 saved_free = free_pages;

        for (u32 word = 0U; word < bitmap_words; ++word)
            allocation_bitmap[word] = ~static_cast<u64>(0U);
        free_pages = 0U;

        paddr_t refused{};
        const error_t exhausted = allocate_physical_page(refused);

        for (u32 word = 0U; word < bitmap_words; ++word)
            allocation_bitmap[word] = snapshot[word];
        free_pages = saved_free;

        /*
         * Fails closed, and says why. `no_memory` rather than a generic
         * error matters because callers branch on it -- a quota-exhausted
         * task is expected to get exactly this and degrade, not fault.
         */
        if (exhausted != error_t::no_memory)
            return error_t::invalid_argument;
        // And it must not hand back an address it did not allocate.
        if (refused != 0U)
            return error_t::invalid_argument;

        // Restoration must leave the allocator usable, not merely intact.
        paddr_t after{};
        if (allocate_physical_page(after) != error_t::success || after == 0U)
            return error_t::invalid_argument;
        if (release_physical_page(after) != error_t::success || free_pages != free_before)
            return error_t::invalid_argument;

        pr_info("[TEST] name=allocator_exhaustion result=PASS burst=%u free=%u "
                "distinct=1 balanced=1 empty_fails_closed=1 recovers=1\n",
                burst, free_before);
        return error_t::success;
    }
} // namespace sys::kernel::tests::allocator_exhaustion
