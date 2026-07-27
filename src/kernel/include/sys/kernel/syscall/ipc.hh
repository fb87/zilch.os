#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/smp.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/ipc/endpoint.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/kernel/verification/hooks.hh>
#include <sys/platform/timer.hh>
#include <sys/types.hh>

#include <abi/sys/v1/syscall_numbers.hh>
#if CONFIG_SELFTEST
#include <sys/test_abi/v1/certification.hh>
#endif

namespace sys::kernel::syscall
{
    inline volatile u64 total_ipc_rendezvous = 0U;
    inline volatile u32 next_reply_nonce = 1U;
#if CONFIG_SELFTEST
    inline volatile u64 total_fuzz_operations = 0U;
    inline volatile u64 total_fuzz_failures = 0U;
    inline constexpr u64 fuzz_progress_interval = 16384U;
    inline volatile u64 next_fuzz_progress = fuzz_progress_interval;
    inline u64 previous_cpu_operations[thread::maximum_cpu_count]{};
    inline u64 previous_cpu_switches[thread::maximum_cpu_count]{};
    inline u64 previous_cpu_ticks[thread::maximum_cpu_count]{};

    [[nodiscard]] inline u64 cpu_fuzz_operations(cpu_id_t cpu) noexcept {
        u64 operations = 0U;
        for (u32 index = 0U; index < thread::user_thread_count; ++index) {
            const thread::thread& value = thread::user_threads[index];
            if (value.pinned_cpu == cpu) {
                operations += __atomic_load_n(&value.fuzz_iterations, __ATOMIC_ACQUIRE);
            }
        }
        return operations;
    }

    inline void log_fuzz_cpu_health(u64 operations) noexcept {
        const u32 online = arch::smp::online_count();
        pr_info("smp fuzz total=%llu failures=%llu rendezvous=%llu cpus=%u\n",
                static_cast<unsigned long long>(operations),
                static_cast<unsigned long long>(
                    __atomic_load_n(&total_fuzz_failures, __ATOMIC_RELAXED)),
                static_cast<unsigned long long>(
                    __atomic_load_n(&total_ipc_rendezvous, __ATOMIC_RELAXED)),
                static_cast<unsigned int>(online));

        const u32 count = online < thread::maximum_cpu_count ? online : thread::maximum_cpu_count;
        bool all_cpus_advanced = count != 0U;
        for (u32 cpu = 0U; cpu < count; ++cpu) {
            const u64 cpu_operations = cpu_fuzz_operations(cpu);
            const u64 switches = __atomic_load_n(&thread::per_cpu_switches[cpu], __ATOMIC_ACQUIRE);
            const u64 ticks = platform::timer::ticks(cpu);
            const bool advanced = cpu_operations > previous_cpu_operations[cpu] &&
                                  switches > previous_cpu_switches[cpu] &&
                                  ticks > previous_cpu_ticks[cpu];
            all_cpus_advanced = all_cpus_advanced && advanced;
            const u32 current =
                __atomic_load_n(&thread::current_user_thread[cpu], __ATOMIC_ACQUIRE);

            pr_info("smp fuzz cpu=%u ops=%llu switches=%llu ticks=%llu current=%u idle=%u "
                    "progress=%s\n",
                    static_cast<unsigned int>(cpu), static_cast<unsigned long long>(cpu_operations),
                    static_cast<unsigned long long>(switches),
                    static_cast<unsigned long long>(ticks), static_cast<unsigned int>(current),
                    thread::user_cpu_idle[cpu] ? 1U : 0U, advanced ? "yes" : "no");

            previous_cpu_operations[cpu] = cpu_operations;
            previous_cpu_switches[cpu] = switches;
            previous_cpu_ticks[cpu] = ticks;
        }
        verification::report_final(
            operations, __atomic_load_n(&total_fuzz_failures, __ATOMIC_ACQUIRE), all_cpus_advanced);
    }
#endif

    [[nodiscard]] inline error_t resolve_endpoint(thread::thread& current, capability_id_t selector,
                                                  capability::right_t required,
                                                  ipc::endpoint*& endpoint) noexcept {
        endpoint = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* object = nullptr;
        const error_t result = capability::lookup(current.owner->cspace, selector,
                                                  object::type_t::endpoint, required, object);
        if (result != error_t::success)
            return result;
        endpoint = reinterpret_cast<ipc::endpoint*>(object);
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

#if CONFIG_SELFTEST
    [[nodiscard]] inline error_t decode_fuzz_result(const thread::thread& current,
                                                    const arch::thread::context& frame) noexcept {
        const word_t endpoint = arch::syscall::argument(frame, 0U);
        const word_t operation = arch::syscall::argument(frame, 1U);
        const word_t id = arch::syscall::argument(frame, 3U);
        if (endpoint != test_abi::v1::debug_endpoint && endpoint != test_abi::v1::fuzz_endpoint)
            return error_t::denied;
        if (operation != static_cast<word_t>(abi::v1::ipc_operation::call))
            return error_t::denied;
        if (id != static_cast<word_t>(current.id))
            return error_t::invalid_argument;
        return error_t::success;
    }

    inline void record_fuzz(thread::thread& current, arch::thread::context& frame,
                            error_t result) noexcept {
        ++current.fuzz_iterations;
        const u64 operations = __atomic_add_fetch(&total_fuzz_operations, 1U, __ATOMIC_RELAXED);
        error_t expected = error_t::unsupported;
        const auto test_case =
            static_cast<test_abi::v1::fuzz_case>(arch::syscall::argument(frame, 2U));
        switch (test_case) {
            case test_abi::v1::fuzz_case::valid_call:
            case test_abi::v1::fuzz_case::random_payload:
                expected = error_t::success;
                break;
            case test_abi::v1::fuzz_case::invalid_capability:
            case test_abi::v1::fuzz_case::invalid_operation:
            case test_abi::v1::fuzz_case::boundary_capability:
                expected = error_t::denied;
                break;
            case test_abi::v1::fuzz_case::wrong_thread_identity:
                expected = error_t::invalid_argument;
                break;
            case test_abi::v1::fuzz_case::mixed:
                expected = result;
                break;
        }
        if (result != expected || !thread::validate(current)) {
            ++current.fuzz_failures;
            __atomic_add_fetch(&total_fuzz_failures, 1U, __ATOMIC_RELAXED);
            pr_err("fuzz failure cpu=%u seed=%llx iteration=%llu thread=%llu case=%llu result=%d "
                   "expected=%d\n",
                   static_cast<unsigned int>(arch::cpu::current_id()),
                   static_cast<unsigned long long>(current.fuzz_seed),
                   static_cast<unsigned long long>(current.fuzz_iterations),
                   static_cast<unsigned long long>(current.id),
                   static_cast<unsigned long long>(arch::syscall::argument(frame, 2U)),
                   static_cast<int>(result), static_cast<int>(expected));
        }
        u64 threshold = __atomic_load_n(&next_fuzz_progress, __ATOMIC_ACQUIRE);
        if (operations >= threshold &&
            __atomic_compare_exchange_n(&next_fuzz_progress, &threshold,
                                        threshold + fuzz_progress_interval, false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            log_fuzz_cpu_health(operations);
        }
    }
#endif

    inline void capture_transfer(thread::thread& value,
                                 const arch::thread::context& frame) noexcept {
        value.transfer = {};
        const word_t descriptor = arch::syscall::argument(frame, 6U);
        if ((descriptor & abi::v1::capability_transfer_valid) == 0U)
            return;
        value.transfer.source = descriptor & 0x3fU;
        value.transfer.destination = (descriptor >> 6U) & 0x3fU;
        value.transfer.rights.bits = static_cast<u32>((descriptor >> 12U) & 0x3fU);
        value.transfer.badge = static_cast<capability::badge_t>(descriptor >> 32U);
        value.transfer.valid = true;
    }

    inline void capture_timeout(thread::thread& value,
                                const arch::thread::context& frame) noexcept {
        const word_t descriptor = arch::syscall::argument(frame, 7U);
        value.ipc_timeout_active = (descriptor & abi::v1::ipc_timeout_valid) != 0U;
        const u64 timeout = descriptor & ~abi::v1::ipc_timeout_valid;
        value.ipc_deadline =
            value.ipc_timeout_active ? platform::timer::ticks(value.pinned_cpu) + timeout : 0U;
    }

    [[nodiscard]] inline error_t transfer_capability(thread::thread& sender,
                                                     thread::thread& receiver) noexcept {
        if (!sender.transfer.valid)
            return error_t::success;
        if (sender.owner == nullptr || receiver.owner == nullptr ||
            sender.transfer.destination >= capability::cspace_slot_count ||
            sender.transfer.source >= capability::cspace_slot_count ||
            sender.transfer.rights.bits == 0U) {
            return error_t::invalid_argument;
        }
        /*
         * Transfer is an authority transaction just like control-path
         * copy/mint/delete/revoke.  Otherwise revoke can finish scanning the
         * tree while this mint publishes a descendant that survives it.
         */
        capability::lock_authority();
        const error_t result = capability::mint_locked(
            receiver.owner->cspace, sender.transfer.destination, sender.owner->cspace,
            sender.transfer.source, sender.transfer.rights, sender.transfer.badge);
        capability::unlock_authority();
        if (result == error_t::success)
            sender.transfer = {};
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
            scheduling::donate_priority(server.scheduling_context, caller.scheduling_context) ==
            error_t::success;
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
                                sender.object.generation, sender.message);
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
        } else if (server.transfer.valid) {
            thread::unlock_ipc_lifecycle();
            return error_t::invalid_argument;
        }

        server.reply = {};
        if (reply.donation_active)
            scheduling::revoke_donation(server.scheduling_context);
        if (caller_state == thread::state::blocked_fault) {
            const auto disposition = static_cast<fault::disposition>(server.message[0]);
            caller.fault_disposition = disposition;
            if (disposition == fault::disposition::resume) {
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
                                server.object.generation, server.message);
        (void)thread::wake(caller);
        ipc::remote_reschedule(caller.pinned_cpu, arch::cpu::current_id());
        thread::unlock_ipc_lifecycle();
        return error_t::success;
    }

    inline bool receive(thread::thread& current, arch::thread::context& frame,
                        capability_id_t selector) noexcept {
        ipc::endpoint* endpoint = nullptr;
        const error_t lookup_result =
            resolve_endpoint(current, selector, capability::right_t::read, endpoint);
        if (lookup_result != error_t::success) {
            set_error(frame, lookup_result);
            return true;
        }
        ipc::lock(*endpoint);
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
            frame.x[1] = static_cast<word_t>(sender.id);
            for (usize_t i = 0U; i < 4U; ++i)
                frame.x[i + 2U] = sender.message[i];
            install_reply(current, sender);
            sender.ipc_timeout_active = false;
            if (sender_state != thread::state::blocked_fault)
                thread::store_state(sender, thread::state::blocked_reply);
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
        const error_t lookup_result =
            resolve_endpoint(current, selector, capability::right_t::write, endpoint);
        if (lookup_result != error_t::success) {
            set_error(frame, lookup_result);
            return true;
        }
        copy_message_from_frame(current, frame);
        capture_transfer(current, frame);
        capture_timeout(current, frame);
        current.waiting_endpoint = selector;
        ipc::lock(*endpoint);
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
                                        current.object.generation, current.message);
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
            current.transfer = {};
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
        if (target.owner != nullptr && target.waiting_endpoint < capability::cspace_slot_count) {
            ipc::endpoint* endpoint = nullptr;
            const capability::right_t endpoint_right =
                target_state == thread::state::blocked_receive ? capability::right_t::read
                                                               : capability::right_t::write;
            if (target_state != thread::state::blocked_reply &&
                resolve_endpoint(target, target.waiting_endpoint, endpoint_right, endpoint) ==
                    error_t::success) {
                (void)ipc::cancel_thread(*endpoint, object::reference(target.object));
            }
        }
        thread::lock_ipc_lifecycle();
        if (thread::load_state(target) != target_state) {
            thread::unlock_ipc_lifecycle();
            set_error(frame, error_t::not_found);
            return true;
        }
        for (u32 index = 0U; index < thread::active_user_thread_count; ++index) {
            thread::thread& server = thread::user_threads[index];
            if (server.reply.valid && server.reply.caller == target.id &&
                server.reply.generation == target.object.generation) {
                if (server.reply.donation_active)
                    scheduling::revoke_donation(server.scheduling_context);
                server.reply = {};
            }
        }
        target.ipc_timeout_active = false;
        target.transfer = {};
        target.pending_result = error_t::timed_out;
        (void)thread::wake(target);
        ipc::remote_reschedule(target.pinned_cpu, arch::cpu::current_id());
        thread::unlock_ipc_lifecycle();
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

#if CONFIG_SELFTEST
        if (arch::syscall::argument(frame, 6U) == test_abi::v1::fuzz_magic) {
            /*
             * x6 is a test-only discriminator, not part of normal IPC.
             * Clear it in the saved context before returning so a caller that
             * forgets to overwrite x6 cannot accidentally classify its next
             * ordinary IPC operation as a fuzz request.
             */
            frame.x[6] = 0U;
            const error_t result = decode_fuzz_result(current, frame);
            record_fuzz(current, frame, result);
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
                capture_transfer(current, frame);
                const error_t result = reply_to_caller(current);
                if (result != error_t::success) {
                    current.transfer = {};
                    set_error(frame, result);
                    return true;
                }
                return receive(current, frame, endpoint);
            }
            case abi::v1::ipc_operation::cancel:
                return cancel(current, frame);
            case abi::v1::ipc_operation::reply: {
                copy_message_from_frame(current, frame);
                capture_transfer(current, frame);
                const error_t result = reply_to_caller(current);
                if (result != error_t::success)
                    current.transfer = {};
                set_error(frame, result);
                return true;
            }
        }
        set_error(frame, error_t::denied);
        return true;
    }
} // namespace sys::kernel::syscall
