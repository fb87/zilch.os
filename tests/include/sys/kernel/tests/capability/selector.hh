#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>

namespace sys::kernel::tests::capability_selector
{
    [[nodiscard]] inline error_t run_selector_width_negative(task::task& root) noexcept {
        using namespace sys::kernel::capability;

        object::header_t* header = nullptr;
        const capability_id_t valid = encode_selector(root.cspace.guard, 10U);
        const capability_id_t alias = valid | (1ULL << 32U);
        const error_t result =
            lookup(root.cspace, alias, object::type_t::endpoint, right_t::write, header);
        if (result != error_t::not_found || header != nullptr)
            return error_t::invalid_argument;

        pr_info("[TEST] name=capability_selector_width result=PASS selector=%llx alias=%llx\n",
                static_cast<unsigned long long>(valid), static_cast<unsigned long long>(alias));
        return error_t::success;
    }
} // namespace sys::kernel::tests::capability_selector
