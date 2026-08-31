#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>

namespace sys::kernel::tests::capability_scale
{
    [[nodiscard]] inline error_t run_basic_lifecycle(task::task& root) noexcept {
        using namespace sys::kernel::capability;
        using namespace sys::kernel::object;

        error_t result = copy(root.cspace, 16U, root.cspace, 10U, rights(right_t::write));
        if (result != error_t::success)
            return result;
        result = move(root.cspace, 17U, root.cspace, 16U);
        if (result != error_t::success)
            return result;
        result = delete_capability(root.cspace, 17U);
        if (result != error_t::success)
            return result;
        result = mint(root.cspace, 18U, root.cspace, 10U, rights(right_t::write), 0x5aU);
        if (result != error_t::success)
            return result;
        const derivation_id_t root_derivation = slot_at(root.cspace, 10U).derivation;
        if (revoke_descendants(root_derivation) != 1U ||
            slot_at(root.cspace, 18U).object.type != type_t::none ||
            slot_at(root.cspace, 10U).object.type == type_t::none)
            return error_t::invalid_argument;
        return error_t::success;
    }

    [[nodiscard]] inline error_t
    run_guarded_scale_and_fuzz(task::task& root, capability::cspace_t& guarded_cspace) noexcept {
        using namespace sys::kernel::capability;
        using namespace sys::kernel::object;

        static capability_id_t scalable_selectors[193]{};
        const derivation_id_t root_derivation = slot_at(root.cspace, 10U).derivation;

        initialize(guarded_cspace);
        error_t result = set_guard(guarded_cspace, 0x5aU);
        if (result != error_t::success)
            return result;
        for (u32 index = 0U; index < 193U; ++index) {
            result = allocate_slot(guarded_cspace, scalable_selectors[index]);
            if (result != error_t::success)
                return result;
            result = copy(guarded_cspace, scalable_selectors[index], root.cspace, 10U,
                          rights(right_t::write));
            if (result != error_t::success)
                return result;
        }
        header_t* guarded_result = nullptr;
        result = lookup(guarded_cspace, scalable_selectors[192U], type_t::endpoint, right_t::write,
                        guarded_result);
        if (result != error_t::success || guarded_result == nullptr ||
            lookup(guarded_cspace, encode_selector(0x5bU, 192U), type_t::endpoint, right_t::write,
                   guarded_result) != error_t::not_found)
            return error_t::invalid_argument;
        if (revoke_descendants(root_derivation) != 193U)
            return error_t::invalid_argument;
        for (u32 leaf = 0U; leaf < cspace_leaf_count; ++leaf) {
            if (guarded_cspace.leaves[leaf].occupied != 0U)
                return error_t::invalid_argument;
        }
        u32 transfer_fuzz_state = 0x7f4a7c15U;
        for (u32 operation = 0U; operation < 4096U; ++operation) {
            transfer_fuzz_state ^= transfer_fuzz_state << 13U;
            transfer_fuzz_state ^= transfer_fuzz_state >> 17U;
            transfer_fuzz_state ^= transfer_fuzz_state << 5U;
            capability_id_t destination{};
            result = allocate_slot(guarded_cspace, destination);
            if (result != error_t::success)
                return result;
            const right_t selected_right =
                (transfer_fuzz_state & 1U) != 0U ? right_t::read : right_t::write;
            result = copy(guarded_cspace, destination, root.cspace, 10U, rights(selected_right));
            if (result != error_t::success)
                return result;
            guarded_result = nullptr;
            result = lookup(guarded_cspace, destination, type_t::endpoint, selected_right,
                            guarded_result);
            if (result != error_t::success || guarded_result == nullptr)
                return error_t::invalid_argument;
            if ((operation & 31U) == 0U &&
                lookup(guarded_cspace,
                       encode_selector(0xa5U,
                                       static_cast<u32>(destination) & (cspace_slot_count - 1U)),
                       type_t::endpoint, selected_right, guarded_result) != error_t::not_found)
                return error_t::invalid_argument;
            result = delete_capability(guarded_cspace, destination);
            if (result != error_t::success)
                return result;
        }
        pr_info("[TEST] name=guarded_cspace_scale result=PASS slots=193 leaves=4 guard=5a\n");
        pr_info("[TEST] name=cross_cspace_transfer_fuzz result=PASS operations=4096\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::capability_scale
