#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/thread/thread.hh>

namespace sys::kernel::tests::ipc
{
    [[nodiscard]] inline error_t run_badge_delivery(task::task& root) noexcept {
        constexpr capability_id_t minted_selector = 18U;
        constexpr capability::badge_t client_badge = 0x51a7c0deU;
        const capability::rights_t send_rights = capability::rights(capability::right_t::write);
        error_t result = capability::mint(root.cspace, minted_selector, root.cspace, 10U,
                                          send_rights, client_badge);
        if (result != error_t::success)
            return result;

        capability::slot_t endpoint_authority{};
        result = capability::lookup_slot(root.cspace, minted_selector, object::type_t::endpoint,
                                         capability::right_t::write, endpoint_authority);
        object::header_t* denied = nullptr;
        if (result != error_t::success || endpoint_authority.badge != client_badge ||
            capability::lookup(root.cspace, minted_selector, object::type_t::endpoint,
                               capability::right_t::read, denied) != error_t::denied)
            return error_t::invalid_argument;

        static thread::thread receiver{};
        receiver.reply = {};
        receiver.pending_sender = static_cast<thread_id_t>(-1);
        receiver.pending_sender_generation = 0U;
        receiver.pending_badge = 0U;
        receiver.pending_ipc_kind = static_cast<u8>(thread::pending_ipc::none);
        word_t message[4];
        message[0] = 0x10U;
        message[1] = 0x20U;
        message[2] = 0x30U;
        message[3] = 0x40U;
        thread::publish_pending(receiver, thread::pending_ipc::incoming_call, 7U, 19U,
                                endpoint_authority.badge, message);

        /*
         * An accepted call owns its badge snapshot. Removing the invoking
         * capability cannot rewrite an already queued or published message.
         */
        result = capability::delete_capability(root.cspace, minted_selector);
        if (result != error_t::success)
            return result;
        thread::consume_pending(receiver);
        if (receiver.context.x[1] != client_badge || receiver.context.x[2] != message[0] ||
            receiver.context.x[5] != message[3] || !receiver.reply.valid ||
            receiver.reply.caller != 7U || receiver.reply.generation != 19U)
            return error_t::invalid_argument;

        pr_info("[TEST] name=ipc_badge_delivery result=PASS badge=%llx sender_hidden=1\n",
                static_cast<unsigned long long>(client_badge));
        pr_info("[TEST] name=ipc_badge_authority_snapshot result=PASS revoke_after_accept=1\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::ipc
