#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::tests::space
{
    [[nodiscard]] inline error_t run_rollover_reuse(thread::thread& root_thread) noexcept {
        const u64 asid_rollovers_before = arch::space::asid::rollovers;
        const u32 asid_generation_before = arch::space::asid::generation;
        for (u32 iteration = 0U; iteration < arch::space::asid::capacity + 4U; ++iteration) {
            arch::space::asid::handle probe{};
            if (arch::space::asid::allocate(probe) != error_t::success)
                return error_t::invalid_argument;
            arch::space::asid::release(probe);
        }
        arch::space::asid::handle root_asid{root_thread.address_space.native.asid,
                                            root_thread.address_space.native.asid_generation};
        if (arch::space::asid::refresh(root_asid) != error_t::success ||
            arch::space::asid::rollovers <= asid_rollovers_before ||
            arch::space::asid::generation == asid_generation_before)
            return error_t::invalid_argument;
        root_thread.address_space.native.asid = root_asid.value;
        root_thread.address_space.native.asid_generation = root_asid.generation;
        pr_info("[TEST] name=asid_rollover_reuse result=PASS generation=%u rollovers=%llu\n",
                static_cast<unsigned int>(root_asid.generation),
                static_cast<unsigned long long>(arch::space::asid::rollovers));
        return error_t::success;
    }
} // namespace sys::kernel::tests::space
