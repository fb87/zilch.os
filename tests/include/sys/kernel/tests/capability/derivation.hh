#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>

namespace sys::kernel::tests::capability_derivation
{
    [[nodiscard]] inline error_t run_derivation_generation_aba(task::task& root) noexcept {
        using namespace sys::kernel::capability;

        const rights_t delegating_endpoint_rights{static_cast<u32>(right_t::write) |
                                                  static_cast<u32>(right_t::grant)};
        error_t result = copy(root.cspace, 18U, root.cspace, 10U, delegating_endpoint_rights);
        if (result != error_t::success)
            return result;
        result = copy(root.cspace, 19U, root.cspace, 18U, delegating_endpoint_rights);
        if (result != error_t::success)
            return result;
        result = copy(root.cspace, 20U, root.cspace, 19U, rights(right_t::write));
        if (result != error_t::success)
            return result;

        const derivation_id_t deleted_ancestor = slot_at(root.cspace, 18U).derivation;
        const derivation_id_t grandchild = slot_at(root.cspace, 20U).derivation;
        result = delete_capability(root.cspace, 18U);
        if (result != error_t::success || !descendant_of(grandchild, deleted_ancestor))
            return error_t::invalid_argument;

        const derivation_id_t saved_hint = next_derivation_hint;
        next_derivation_hint = derivation_index(deleted_ancestor);
        const derivation_id_t reuse_probe =
            allocate_derivation(0U, slot_at(root.cspace, 10U).object);
        next_derivation_hint = saved_hint;
        if (reuse_probe == 0U ||
            derivation_index(reuse_probe) == derivation_index(deleted_ancestor)) {
            deactivate_derivation(reuse_probe);
            return error_t::invalid_argument;
        }
        deactivate_derivation(reuse_probe);

        if (revoke_descendants(deleted_ancestor) != 2U ||
            slot_at(root.cspace, 19U).object.type != object::type_t::none ||
            slot_at(root.cspace, 20U).object.type != object::type_t::none)
            return error_t::invalid_argument;

        pr_info("[TEST] name=derivation_generation_aba result=PASS ancestor=%llx probe=%llx\n",
                static_cast<unsigned long long>(deleted_ancestor),
                static_cast<unsigned long long>(reuse_probe));
        return error_t::success;
    }
} // namespace sys::kernel::tests::capability_derivation
