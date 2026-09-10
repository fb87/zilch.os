#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::tests::space
{
    /*
     * Forces a real rollover by EXHAUSTING the tag pool -- holding every
     * handle it takes -- and then checks that a stale handle refreshes onto
     * the new generation.
     *
     * It used to allocate and immediately release, `capacity + 4` times,
     * and expect that to roll over. That only worked because the allocator
     * counted LIFETIME allocations: release() cleared the tag's in_use bit
     * but left the counter raised, so 68 allocate/release pairs "exhausted"
     * a pool that never had more than one tag outstanding. The counter now
     * tracks live allocations, which is correct, and churn like that
     * rightly never rolls over -- so the old loop tested nothing and would
     * have gone on passing while the property it names stopped holding.
     * Holding the handles is what actually exhausts the pool.
     */
    [[nodiscard]] inline error_t run_rollover_reuse(thread::thread& root_thread) noexcept {
        const u64 asid_rollovers_before = arch::space::asid::rollovers;
        const u32 asid_generation_before = arch::space::asid::generation;

        constexpr u32 attempts = arch::space::asid::capacity + 4U;
        arch::space::asid::handle probe{};
        for (u32 iteration = 0U;
             iteration < attempts && arch::space::asid::rollovers == asid_rollovers_before;
             ++iteration) {
            if (arch::space::asid::allocate(probe) != error_t::success)
                break;
        }
        const bool rolled = arch::space::asid::rollovers > asid_rollovers_before &&
                            arch::space::asid::generation != asid_generation_before;
        /*
         * Only the last handle needs releasing, and only one is held: every
         * tag taken before the rollover was freed BY the rollover, and
         * release() ignores their stale generation anyway. Done before the
         * assertion so a failure here cannot also strand the pool for
         * everything that runs after it. (No array of handles, deliberately
         * -- a zero-initialised one compiles to a memset call this
         * freestanding kernel does not link.)
         */
        arch::space::asid::release(probe);
        if (!rolled)
            return error_t::invalid_argument;

        arch::space::asid::handle root_asid{root_thread.address_space.native.asid,
                                            root_thread.address_space.native.asid_generation};
        if (arch::space::asid::refresh(root_asid) != error_t::success)
            return error_t::invalid_argument;
        root_thread.address_space.native.asid = root_asid.value;
        root_thread.address_space.native.asid_generation = root_asid.generation;
        pr_info("[TEST] name=asid_rollover_reuse result=PASS generation=%u rollovers=%llu\n",
                static_cast<unsigned int>(root_asid.generation),
                static_cast<unsigned long long>(arch::space::asid::rollovers));
        return error_t::success;
    }
} // namespace sys::kernel::tests::space
