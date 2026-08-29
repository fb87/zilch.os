#pragma once

#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::object_generation
{
    [[nodiscard]] inline error_t run_generation_retirement() noexcept {
        using namespace sys::kernel::object;

        constexpr object_id_t target = table_capacity - 2U;
        table_slot_t saved = object_table[target];
        header_t probe{};
        object_table[target].object = nullptr;
        object_table[target].generation = ~static_cast<u32>(0U);

        const error_t result = register_object(probe, target, type_t::endpoint);
        object_table[target] = saved;
        if (result != error_t::no_memory || probe.type != type_t::none || probe.generation != 0U)
            return error_t::invalid_argument;

        pr_info("[TEST] name=object_generation_retirement result=PASS target=%u\n",
                static_cast<unsigned int>(target));
        return error_t::success;
    }
} // namespace sys::kernel::tests::object_generation
