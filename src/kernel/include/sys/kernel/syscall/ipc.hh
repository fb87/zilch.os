#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/smp.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/ipc/endpoint.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/kernel/user_access.hh>
#include <sys/kernel/verification/hooks.hh>
#include <sys/platform/timer.hh>
#include <sys/types.hh>

#include <abi/sys/v1/ipc.hh>
#include <abi/sys/v1/syscall_numbers.hh>
#if CONFIG_SELFTEST
#include <sys/kernel/tests/certification/fuzz.hh>
#include <sys/test_abi/v1/certification.hh>
#endif

namespace sys::kernel::syscall
{
    inline volatile u64 total_ipc_rendezvous = 0U;
    inline volatile u32 next_reply_nonce = 1U;

    [[nodiscard]] inline error_t resolve_endpoint(thread::thread& current, capability_id_t selector,
                                                  capability::right_t required,
                                                  ipc::endpoint*& endpoint,
                                                  capability::badge_t& badge) noexcept {
        endpoint = nullptr;
        badge = 0U;
        if (current.owner == nullptr)
            return error_t::denied;
        capability::slot_t slot{};
        const error_t result = capability::lookup_slot(current.owner->cspace, selector,
                                                       object::type_t::endpoint, required, slot);
        if (result != error_t::success)
            return result;
        object::header_t* object = object::resolve(slot.object);
        if (object == nullptr)
            return error_t::not_found;
        endpoint = reinterpret_cast<ipc::endpoint*>(object);
        badge = slot.badge;
        return error_t::success;
    }

    [[nodiscard]] inline thread::thread*
    resolve_thread_reference(const object::reference_t& reference) noexcept {
        if (reference.type != object::type_t::thread)
            return nullptr;
        object::header_t* header = object::resolve(reference);
        if (header == nullptr)
            return nullptr;
        return reinterpret_cast<thread::thread*>(header);
    }

    inline void set_error(arch::thread::context& frame, error_t result) noexcept {
        arch::syscall::set_result(frame, static_cast<word_t>(static_cast<s64>(result)));
    }

    [[nodiscard]] inline error_t capture_transfer(thread::thread& value,
                                                  const arch::thread::context& frame) noexcept {
        thread::clear_transfer(value.transfer);
        const word_t descriptor = arch::syscall::argument(frame, 6U);
        if ((descriptor & abi::v1::capability_transfer_valid) == 0U)
            return error_t::success;
        if ((descriptor & abi::v1::capability_transfer_batch) != 0U) {
            abi::v1::ipc_transfer_batch batch{};
            const vaddr_t address = descriptor & ~(abi::v1::capability_transfer_valid |
                                                   abi::v1::capability_transfer_batch);
            const error_t copied = user_access::copy_from_user(value.address_space.native, &batch,
                                                               address, sizeof(batch));
            if (copied != error_t::success || batch.count == 0U ||
                batch.count > abi::v1::maximum_capability_transfers)
                return error_t::invalid_argument;
            value.transfer.count = static_cast<u32>(batch.count);
            for (u32 index = 0U; index < value.transfer.count; ++index) {
                const abi::v1::ipc_transfer& source = batch.entries[index];
                if (source.source == static_cast<capability_id_t>(-1))
                    return error_t::invalid_argument;
                thread::capability_transfer& transfer = value.transfer.entries[index];
                transfer.source = source.source;
                transfer.destination = source.destination;
                transfer.rights.bits = source.rights;
                transfer.badge = source.badge;
                transfer.valid = true;
            }
            return error_t::success;
        }
        value.transfer.count = 1U;
        thread::capability_transfer& transfer = value.transfer.entries[0];
        transfer.source = descriptor & 0x3fU;
        transfer.destination = (descriptor >> 6U) & 0x3fU;
        transfer.rights.bits = static_cast<u32>((descriptor >> 12U) & 0x3fU);
        transfer.badge = static_cast<capability::badge_t>(descriptor >> 32U);
        transfer.valid = true;
        return error_t::success;
    }

    inline void capture_timeout(thread::thread& value,
                                const arch::thread::context& frame) noexcept {
        const word_t descriptor = arch::syscall::argument(frame, 7U);
        value.ipc_timeout_active = (descriptor & abi::v1::ipc_timeout_valid) != 0U;
        const u64 timeout = descriptor & ~abi::v1::ipc_timeout_valid;
        value.ipc_deadline =
            value.ipc_timeout_active
                ? platform::timer::deadline_after(platform::timer::ticks(value.pinned_cpu), timeout)
                : 0U;
        if (value.ipc_timeout_active)
            thread::arm_ipc_timeout(value);
    }

    [[nodiscard]] inline error_t transfer_capability(thread::thread& sender,
                                                     thread::thread& receiver) noexcept {
        if (!sender.transfer.valid())
            return error_t::success;
        if (sender.owner == nullptr || receiver.owner == nullptr)
            return error_t::invalid_argument;
        capability::lock_authority();
        error_t result = error_t::success;
        u32 installed = 0U;
        for (u32 index = 0U; index < sender.transfer.count; ++index) {
            const thread::capability_transfer& transfer = sender.transfer.entries[index];
            if (!transfer.valid || transfer.destination >= capability::cspace_slot_count ||
                transfer.source >= capability::cspace_slot_count || transfer.rights.bits == 0U) {
                result = error_t::invalid_argument;
                break;
            }
            for (u32 previous = 0U; previous < index; ++previous)
                if (sender.transfer.entries[previous].destination == transfer.destination)
                    result = error_t::invalid_argument;
            if (result != error_t::success)
                break;
            result = capability::mint_locked(receiver.owner->cspace, transfer.destination,
                                             sender.owner->cspace, transfer.source, transfer.rights,
                                             transfer.badge);
            if (result != error_t::success)
                break;
            ++installed;
        }
        if (result != error_t::success)
            while (installed != 0U)
                (void)capability::delete_capability_locked(
                    receiver.owner->cspace, sender.transfer.entries[--installed].destination);
        capability::unlock_authority();
        if (result == error_t::success)
            thread::clear_transfer(sender.transfer);
        return result;
    }

    inline void install_reply(thread::thread& server, thread::thread& caller) noexcept {
        server.reply.caller = caller.id;
        server.reply.generation = caller.object.generation;
        server.reply.nonce = __atomic_fetch_add(&next_reply_nonce, 1U, __ATOMIC_ACQ_REL);
        if (server.reply.nonce == 0U) {
            server.reply.nonce = __atomic_fetch_add(&next_reply_nonce, 1U, __ATOMIC_ACQ_REL);
        }
        server.reply.donation_active =
            scheduling::donate(server.scheduling_context, caller.scheduling_context,
                               platform::timer::ticks(arch::cpu::current_id())) == error_t::success;
        server.reply.valid = true;
    }

    inline void copy_message_from_frame(thread::thread& value,
                                        const arch::thread::context& frame) noexcept {
        for (usize_t i = 0U; i < 4U; ++i) {
            value.message[i] = arch::syscall::argument(frame, i + 2U);
        }
    }

    [[nodiscard]] inline error_t deliver_to_receiver(thread::thread& receiver,
                                                     thread::thread& sender) noexcept {
        const error_t transfer_result = transfer_capability(sender, receiver);
        if (transfer_result != error_t::success)
            return transfer_result;
        install_reply(receiver, sender);
        thread::publish_pending(receiver, thread::pending_ipc::incoming_call, sender.id,
                                sender.object.generation, sender.ipc_badge, sender.message);
        sender.ipc_badge = 0U;
        (void)thread::wake(receiver);
        ipc::remote_reschedule(receiver.pinned_cpu, arch::cpu::current_id());
        return error_t::success;
    }

    [[nodiscard]] inline error_t reply_to_caller(thread::thread& server) noexcept {
        thread::lock_ipc_lifecycle();
        const thread::reply_capability reply = server.reply;
        if (!reply.valid || reply.caller >= thread::user_thread_count) {
            thread::unlock_ipc_lifecycle();
            return error_t::not_found;
        }
        thread::thread& caller = thread::user_threads[reply.caller];
        const thread::state caller_state = thread::load_state(caller);
        if (caller.object.generation != reply.generation ||
            (caller_state != thread::state::blocked_reply &&
             caller_state != thread::state::blocked_fault)) {
            thread::unlock_ipc_lifecycle();
            return error_t::not_found;
        }

        /*
         * Reply capability transfer is committed before consuming the
         * single-use reply authority.  A busy or invalid destination leaves
         * the caller blocked and allows the server to retry or cancel.
         */
        if (caller_state == thread::state::blocked_reply) {
            const error_t transfer_result = transfer_capability(server, caller);
            if (transfer_result != error_t::success) {
                thread::unlock_ipc_lifecycle();
                return transfer_result;
            }
        } else if (server.transfer.valid()) {
            thread::unlock_ipc_lifecycle();
            return error_t::invalid_argument;
        }

        server.reply = {};
        if (reply.donation_active)
            scheduling::revoke_donation(server.scheduling_context, caller.scheduling_context);
        if (caller_state == thread::state::blocked_fault) {
            const auto disposition = static_cast<fault::disposition>(server.message[0]);
            caller.fault_disposition = disposition;
            caller.ipc_timeout_active = false;
            caller.waiting_endpoint = 0U;
            if (disposition == fault::disposition::resume) {
                caller.last_fault = {};
                (void)thread::wake(caller);
                ipc::remote_reschedule(caller.pinned_cpu, arch::cpu::current_id());
            } else {
                thread::store_state(caller, thread::state::terminated);
            }
            thread::unlock_ipc_lifecycle();
            return error_t::success;
        }
        caller.ipc_timeout_active = false;
        thread::publish_pending(caller, thread::pending_ipc::reply, server.id,
                                server.object.generation, 0U, server.message);
        (void)thread::wake(caller);
        ipc::remote_reschedule(caller.pinned_cpu, arch::cpu::current_id());
        thread::unlock_ipc_lifecycle();
        return error_t::success;
    }

    inline bool receive(thread::thread& current, arch::thread::context& frame,
                        capability_id_t selector) noexcept {
        ipc::endpoint* endpoint = nullptr;
        capability::badge_t ignored_badge{};
        const error_t lookup_result =
            resolve_endpoint(current, selector, capability::right_t::read, endpoint, ignored_badge);
        if (lookup_result != error_t::success) {
            set_error(frame, lookup_result);
            return true;
        }
        ipc::lock(*endpoint);
        if (endpoint->retiring) {
            ipc::unlock(*endpoint);
            set_error(frame, error_t::not_found);
            return true;
        }
        object::reference_t sender_reference{};
        while (ipc::dequeue_sender(*endpoint, sender_reference)) {
            thread::thread* sender_pointer = resolve_thread_reference(sender_reference);
            if (sender_pointer == nullptr)
                continue;
            thread::thread& sender = *sender_pointer;
            thread::lock_ipc_lifecycle();
            const thread::state sender_state = thread::load_state(sender);
            if (sender_state != thread::state::blocked_send &&
                sender_state != thread::state::blocked_fault) {
                thread::unlock_ipc_lifecycle();
                continue;
            }
            const error_t transfer_result = transfer_capability(sender, current);
            if (transfer_result != error_t::success) {
                sender.pending_result = transfer_result;
                sender.ipc_timeout_active = false;
                (void)thread::wake(sender);
                thread::unlock_ipc_lifecycle();
                ipc::remote_reschedule(sender.pinned_cpu, arch::cpu::current_id());
                continue;
            }
            set_error(frame, error_t::success);
            frame.x[1] = static_cast<word_t>(sender.ipc_badge);
            sender.ipc_badge = 0U;
            for (usize_t i = 0U; i < 4U; ++i)
                frame.x[i + 2U] = sender.message[i];
            install_reply(current, sender);
            if (sender_state != thread::state::blocked_fault) {
                sender.ipc_timeout_active = false;
                thread::store_state(sender, thread::state::blocked_reply);
            }
            thread::unlock_ipc_lifecycle();
            __atomic_add_fetch(&total_ipc_rendezvous, 1U, __ATOMIC_RELAXED);
            const bool valid = ipc::validate(*endpoint);
            ipc::unlock(*endpoint);
            if (!valid)
                pr_err("endpoint invariant failed selector=%llu\n",
                       static_cast<unsigned long long>(selector));
            return true;
        }
        if (endpoint->receiver.type != object::type_t::none) {
            ipc::unlock(*endpoint);
            set_error(frame, error_t::busy);
            return true;
        }
        /*
         * Publish the blocked state and saved context before exposing this
         * thread as the endpoint receiver.  Otherwise a remote caller can
         * wake us and then this CPU can overwrite ready with blocked_receive.
         */
        current.waiting_endpoint = selector;
        capture_timeout(current, frame);
        thread::prepare_block(frame, thread::state::blocked_receive);
        endpoint->receiver = object::reference(current.object);
        ipc::unlock(*endpoint);
        thread::schedule_prepared(frame);
        return true;
    }

    inline bool call(thread::thread& current, arch::thread::context& frame,
                     capability_id_t selector) noexcept {
        ipc::endpoint* endpoint = nullptr;
        capability::badge_t endpoint_badge{};
        const error_t lookup_result = resolve_endpoint(
            current, selector, capability::right_t::write, endpoint, endpoint_badge);
        if (lookup_result != error_t::success) {
            set_error(frame, lookup_result);
            return true;
        }
        copy_message_from_frame(current, frame);
        const error_t capture_result = capture_transfer(current, frame);
        if (capture_result != error_t::success) {
            set_error(frame, capture_result);
            return true;
        }
        capture_timeout(current, frame);
        current.waiting_endpoint = selector;
        current.ipc_badge = endpoint_badge;
        ipc::lock(*endpoint);
        if (endpoint->retiring) {
            current.ipc_timeout_active = false;
            thread::clear_transfer(current.transfer);
            current.ipc_badge = 0U;
            ipc::unlock(*endpoint);
            set_error(frame, error_t::not_found);
            return true;
        }
        if (endpoint->receiver.type != object::type_t::none) {
            const object::reference_t receiver_reference = endpoint->receiver;
            endpoint->receiver = {};
            thread::thread* receiver_pointer = resolve_thread_reference(receiver_reference);
            if (receiver_pointer != nullptr) {
                thread::thread& receiver = *receiver_pointer;
                thread::lock_ipc_lifecycle();
                if (thread::load_state(receiver) != thread::state::blocked_receive) {
                    thread::unlock_ipc_lifecycle();
                    goto enqueue_sender;
                }

                /*
                 * The caller must become blocked before the receiver is made
                 * runnable.  A receiver on another CPU may reply immediately.
                 */
                const error_t transfer_result = transfer_capability(current, receiver);
                if (transfer_result != error_t::success) {
                    endpoint->receiver = receiver_reference;
                    thread::unlock_ipc_lifecycle();
                    ipc::unlock(*endpoint);
                    set_error(frame, transfer_result);
                    return true;
                }
                thread::prepare_block(frame, thread::state::blocked_reply);
                install_reply(receiver, current);
                thread::publish_pending(receiver, thread::pending_ipc::incoming_call, current.id,
                                        current.object.generation, current.ipc_badge,
                                        current.message);
                current.ipc_badge = 0U;
                (void)thread::wake(receiver);
                thread::unlock_ipc_lifecycle();
                ipc::remote_reschedule(receiver.pinned_cpu, arch::cpu::current_id());
                __atomic_add_fetch(&total_ipc_rendezvous, 1U, __ATOMIC_RELAXED);
                ipc::unlock(*endpoint);
                thread::schedule_prepared(frame);
                return true;
            }
        }

        /*
         * Publish blocked_send before placing the caller in the sender queue.
         * A remote receiver may dequeue, reply, and wake this thread before
         * this CPU has left the syscall path.
         */
    enqueue_sender:
        thread::prepare_block(frame, thread::state::blocked_send);
        if (!ipc::enqueue_sender(*endpoint, object::reference(current.object))) {
            (void)thread::compare_state(current, thread::state::blocked_send,
                                        thread::state::running);
            current.ipc_timeout_active = false;
            thread::clear_transfer(current.transfer);
            ipc::unlock(*endpoint);
            set_error(frame, error_t::busy);
            return true;
        }
        ipc::unlock(*endpoint);
        thread::schedule_prepared(frame);
        return true;
    }

    inline bool cancel(thread::thread& current, arch::thread::context& frame) noexcept {
        if (current.owner == nullptr) {
            set_error(frame, error_t::denied);
            return true;
        }
        object::header_t* target_header = nullptr;
        const capability_id_t selector = arch::syscall::argument(frame, 2U);
        const error_t lookup_result =
            capability::lookup(current.owner->cspace, selector, object::type_t::thread,
                               capability::right_t::control, target_header);
        if (lookup_result != error_t::success || target_header == nullptr) {
            set_error(frame, lookup_result);
            return true;
        }
        auto& target = *reinterpret_cast<thread::thread*>(target_header);
        const thread::state target_state = thread::load_state(target);
        if (target_state != thread::state::blocked_send &&
            target_state != thread::state::blocked_receive &&
            target_state != thread::state::blocked_reply) {
            set_error(frame, error_t::not_found);
            return true;
        }
        const capability_id_t waited_selector = target.waiting_endpoint;
        ipc::endpoint* waited_endpoint = nullptr;
        if (target.owner != nullptr && waited_selector < capability::cspace_slot_count) {
            capability::badge_t ignored_badge{};
            const capability::right_t endpoint_right =
                target_state == thread::state::blocked_receive ? capability::right_t::read
                                                               : capability::right_t::write;
            if (target_state != thread::state::blocked_reply &&
                resolve_endpoint(target, waited_selector, endpoint_right, waited_endpoint,
                                 ignored_badge) == error_t::success) {
                ipc::lock(*waited_endpoint);
            }
        }
        thread::lock_ipc_lifecycle();
        if (thread::load_state(target) != target_state ||
            target.waiting_endpoint != waited_selector ||
            (target_state != thread::state::blocked_reply && waited_endpoint == nullptr)) {
            thread::unlock_ipc_lifecycle();
            if (waited_endpoint != nullptr)
                ipc::unlock(*waited_endpoint);
            set_error(frame, error_t::not_found);
            return true;
        }
        if (waited_endpoint != nullptr)
            (void)ipc::cancel_thread_locked(*waited_endpoint, object::reference(target.object));
        for (u32 index = 0U; index < thread::active_user_thread_count; ++index) {
            thread::thread& server = thread::user_threads[index];
            if (server.reply.valid && server.reply.caller == target.id &&
                server.reply.generation == target.object.generation) {
                if (server.reply.donation_active)
                    scheduling::revoke_donation(server.scheduling_context,
                                                target.scheduling_context);
                server.reply = {};
            }
        }
        target.ipc_timeout_active = false;
        thread::clear_transfer(target.transfer);
        target.ipc_badge = 0U;
        target.pending_result = error_t::timed_out;
        (void)thread::wake(target);
        ipc::remote_reschedule(target.pinned_cpu, arch::cpu::current_id());
        thread::unlock_ipc_lifecycle();
        if (waited_endpoint != nullptr)
            ipc::unlock(*waited_endpoint);
        set_error(frame, error_t::success);
        return true;
    }

    [[nodiscard]] inline bool dispatch_ipc(thread::thread& current, arch::thread::context& frame,
                                           u64 vector, u64 syndrome) noexcept {
        if (!arch::syscall::is_user_syscall(vector, syndrome))
            return false;
        if (arch::syscall::number(frame) != static_cast<word_t>(abi::v1::syscall::ipc)) {
            set_error(frame, error_t::unsupported);
            return true;
        }
        const interrupt::timing::latency_scope ipc_latency{
            interrupt::timing::latency_kind::ipc_service};

#if CONFIG_SELFTEST
        if (arch::syscall::argument(frame, 6U) == test_abi::v1::fuzz_magic) {
            /*
             * Argument 6 is a test-only discriminator, not part of normal
             * IPC. Clear it in the saved context before returning so a
             * caller that forgets to overwrite it cannot accidentally
             * classify its next ordinary IPC operation as a fuzz request.
             */
            arch::syscall::set_output(frame, 6U, 0U);
            const error_t result = tests::certification::decode_fuzz_result(current, frame);
            tests::certification::record_fuzz(
                current, frame, result, __atomic_load_n(&total_ipc_rendezvous, __ATOMIC_RELAXED));
            set_error(frame, result);
            return true;
        }
#endif

        const auto operation =
            static_cast<abi::v1::ipc_operation>(arch::syscall::argument(frame, 1U));
        const capability_id_t endpoint = arch::syscall::argument(frame, 0U);
        emergency::trace(emergency::event::ipc, current.id, static_cast<u64>(operation), endpoint);
        switch (operation) {
            case abi::v1::ipc_operation::call:
                return call(current, frame, endpoint);
            case abi::v1::ipc_operation::receive:
                return receive(current, frame, endpoint);
            case abi::v1::ipc_operation::reply_receive: {
                copy_message_from_frame(current, frame);
                const error_t capture_result = capture_transfer(current, frame);
                if (capture_result != error_t::success) {
                    set_error(frame, capture_result);
                    return true;
                }
                const error_t result = reply_to_caller(current);
                if (result != error_t::success) {
                    thread::clear_transfer(current.transfer);
                    set_error(frame, result);
                    return true;
                }
                return receive(current, frame, endpoint);
            }
            case abi::v1::ipc_operation::cancel:
                return cancel(current, frame);
            case abi::v1::ipc_operation::reply: {
                copy_message_from_frame(current, frame);
                const error_t capture_result = capture_transfer(current, frame);
                if (capture_result != error_t::success) {
                    set_error(frame, capture_result);
                    return true;
                }
                const error_t result = reply_to_caller(current);
                if (result != error_t::success)
                    thread::clear_transfer(current.transfer);
                set_error(frame, result);
                return true;
            }
        }
        set_error(frame, error_t::denied);
        return true;
    }
} // namespace sys::kernel::syscall
