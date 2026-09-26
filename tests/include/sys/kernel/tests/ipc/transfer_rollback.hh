#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/syscall/ipc.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/verification/hooks.hh>

namespace sys::kernel::tests::transfer_rollback
{
    /*
     * IPC capability transfer is all-or-nothing, under injected failure
     * (TST-018).
     *
     * CSPACE_DESIGN.md states the contract: a call or reply carries at most
     * four capabilities, and "any duplicate, occupied destination, or mint
     * failure rolls back the complete batch". The duplicate and occupied
     * cases are reachable by passing bad arguments and are covered. A mint
     * FAILURE partway through a batch is not reachable that way -- it needs
     * the derivation table to be exhausted at exactly the right moment --
     * so the rollback loop that exists for it had never run.
     *
     * That loop is the whole contract. A receiver that got two of three
     * capabilities and an error would be in a state no caller is written to
     * handle: it never asked for a partial grant and has no way to learn
     * which parts arrived.
     */
    [[nodiscard]] inline error_t run(task::task& root) noexcept
    {
        using verification::injection_site;
        constexpr capability_id_t source_selector = 10U; // root's fault endpoint
        constexpr capability_id_t destinations[] = {40U, 41U, 42U};
        constexpr u32 batch = 3U;

        // Every destination must start free, or a "rolled back" result below
        // would be indistinguishable from one that never installed anything.
        for (const capability_id_t destination : destinations) {
            if (capability::slot_at(root.cspace, destination).object.type != object::type_t::none)
                return error_t::invalid_argument;
        }
        if (capability::slot_at(root.cspace, source_selector).object.type == object::type_t::none)
            return error_t::invalid_argument; // nothing to transfer

        static thread::thread sender{};
        static thread::thread receiver{};
        sender.owner = &root;
        receiver.owner = &root;

        const auto arm_batch = [&]() noexcept {
            thread::clear_transfer(sender.transfer);
            for (u32 index = 0U; index < batch; ++index) {
                auto& entry = sender.transfer.entries[index];
                entry.source = source_selector;
                entry.destination = destinations[index];
                entry.rights = capability::rights(capability::right_t::read,
                                                  capability::right_t::write);
                entry.badge = 0x5eed0000U + index;
                entry.valid = true;
            }
            sender.transfer.count = batch;
        };

        /*
         * Fail the SECOND mint: one capability is already installed when the
         * batch aborts, so the rollback has real work to do. Failing the
         * first would exercise nothing.
         */
        arm_batch();
        verification::configure_failure(injection_site::capability_mint, 2U);
        const error_t partial = syscall::transfer_capability(sender, receiver);
        verification::configure_failure(injection_site::capability_mint, 0U);

        if (partial == error_t::success)
            return error_t::invalid_argument; // injection did not take
        for (const capability_id_t destination : destinations) {
            if (capability::slot_at(root.cspace, destination).object.type != object::type_t::none)
                return error_t::invalid_argument; // a partial grant survived
        }

        /*
         * The same batch must then succeed completely once nothing is armed
         * -- a rollback that corrupted the derivation table or left a slot
         * half-claimed shows up here rather than above.
         */
        arm_batch();
        if (syscall::transfer_capability(sender, receiver) != error_t::success)
            return error_t::invalid_argument;
        for (const capability_id_t destination : destinations) {
            if (capability::slot_at(root.cspace, destination).object.type == object::type_t::none)
                return error_t::invalid_argument;
        }

        // Leave the CSpace as found.
        for (const capability_id_t destination : destinations) {
            if (capability::delete_capability(root.cspace, destination) != error_t::success)
                return error_t::invalid_argument;
        }

        pr_info("[TEST] name=ipc_transfer_batch_rollback result=PASS batch=%u failed_at=2 "
                "fully_rolled_back=1 retry_succeeds=1\n",
                batch);
        return error_t::success;
    }
} // namespace sys::kernel::tests::transfer_rollback
