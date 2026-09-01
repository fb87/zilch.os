#pragma once

/*
 * NOT a standalone-includable header: like fault/lifecycle.hh (see that
 * file's own header comment), this depends on sys::kernel::thread-internal
 * symbols (create_user_bundle, destroy_user_bundle, store_state,
 * load_state, thread::thread's fields) declared earlier in scheduler.hh --
 * must be included from that file, after those declarations.
 */
namespace sys::kernel::tests::fault_lifecycle
{
    /*
     * destroy_user_bundle() forcibly tears down a task/thread bundle. If
     * that thread owed a reply to some other caller (thread::reply.valid),
     * nothing used to notice or wake that caller -- it would stay
     * blocked_reply forever, since the only place that ever woke a pending
     * reply was thread_exit()'s syscall handler, for a *graceful* exit.
     * release_pending_reply() (scheduler.hh, called from
     * destroy_user_bundle() right after quiesce_user_thread() succeeds)
     * fixes this. Never exercised before restart-on-fault (USR-034): that's
     * the first thing in this codebase that calls process_destroy on a task
     * genuinely mid-reply to someone else (root's blocking ipc_call into
     * domain-manager's serve() operation, when a fault-triggered restart
     * destroys the domain-manager task out from under that call).
     */
    [[nodiscard]] inline error_t run_reply_wakeup(task::task& root) noexcept {
        // Selectors 50-55: clear of root's permanent bootstrap capabilities
        // (1-4, 10, 12, 14, 15, 28, 29, 32 -- scheduler.hh's root bootstrap)
        // and of fault_lifecycle.hh's/badge_delivery's transient 18-23.
        error_t result =
            thread::create_user_bundle(root, 1U, thread::fault_client_role, 50U, 51U, 52U);
        if (result != error_t::success)
            return result;
        result = thread::create_user_bundle(root, 2U, thread::fault_client_role, 53U, 54U, 55U);
        if (result != error_t::success)
            return result;

        object::header_t* server_header = nullptr;
        result = capability::lookup(root.cspace, 50U, object::type_t::thread,
                                    capability::right_t::control, server_header);
        if (result != error_t::success || server_header == nullptr)
            return error_t::invalid_argument;
        object::header_t* caller_header = nullptr;
        result = capability::lookup(root.cspace, 53U, object::type_t::thread,
                                    capability::right_t::control, caller_header);
        if (result != error_t::success || caller_header == nullptr)
            return error_t::invalid_argument;
        auto& server = *reinterpret_cast<thread::thread*>(server_header);
        auto& caller = *reinterpret_cast<thread::thread*>(caller_header);

        // server owes caller a reply -- exactly the state a real ipc_call/
        // ipc_receive rendezvous leaves behind, just constructed directly
        // (same technique fault_lifecycle.hh uses for blocked_fault state).
        server.reply.caller = caller.id;
        server.reply.generation = caller.object.generation;
        server.reply.donation_active = false;
        server.reply.valid = true;
        thread::store_state(caller, thread::state::blocked_reply);

        thread::store_state(server, thread::state::suspended);
        result = thread::destroy_user_bundle(root, 50U, 51U, 52U);
        if (result != error_t::success)
            return result;

        if (thread::load_state(caller) != thread::state::ready ||
            caller.pending_result != error_t::timed_out) {
            return error_t::invalid_argument;
        }

        thread::store_state(caller, thread::state::suspended);
        result = thread::destroy_user_bundle(root, 53U, 54U, 55U);
        if (result != error_t::success)
            return result;

        pr_info("[TEST] name=destroy_wakes_pending_reply result=PASS disposition=timed_out\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::fault_lifecycle
