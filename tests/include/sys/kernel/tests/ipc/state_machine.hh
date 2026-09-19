#pragma once

#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/syscall/ipc.hh>
#include <sys/kernel/thread/thread.hh>

namespace sys::kernel::tests::ipc
{
    /*
     * Properties of the IPC state machine that are contracts rather than
     * implementation details -- the ones an independent implementation would
     * have to reproduce, and the ones whose violation produces a silent hang
     * instead of an error. See docs/readiness/CAPABILITY_IPC_SEMANTICS.md.
     *
     * These are deliberately unit-level, driving the kernel structures
     * directly rather than going through a live rendezvous: the point is to
     * pin the rules, and the integration behaviour is already covered by
     * ipc_lifecycle_races and the fault-reply suites.
     */
    [[nodiscard]] inline error_t run_state_machine() noexcept {
        static thread::thread server{};
        static thread::thread first_caller{};
        static thread::thread second_caller{};

        server.reply = {};
        first_caller.id = 11U;
        first_caller.object.generation = 3U;
        second_caller.id = 12U;
        second_caller.object.generation = 4U;

        /*
         * A reply capability identifies its caller by id AND generation.
         * Generation is what makes a stale reply fail closed rather than
         * answering whichever thread later reused the slot.
         */
        syscall::install_reply(server, first_caller);
        if (!server.reply.valid || server.reply.caller != first_caller.id ||
            server.reply.generation != first_caller.object.generation)
            return error_t::invalid_argument;

        /*
         * The nonce is never zero. install_reply() retries specifically to
         * avoid it, because zero is the "no reply outstanding" value and a
         * zero-nonce reply would be indistinguishable from none.
         */
        const u64 first_nonce = server.reply.nonce;
        if (first_nonce == 0U)
            return error_t::invalid_argument;

        /*
         * THE DEFERRED-REPLY HAZARD, pinned deliberately.
         *
         * A server thread has exactly one reply slot and installation
         * overwrites it unconditionally. This is not a bug to be fixed here
         * -- it is the contract -- but it is the contract that produced two
         * real defects in this tree, where a server deferred a reply and the
         * next call to the same thread destroyed it, stranding the original
         * caller forever.
         *
         * Asserting it means a future change that makes installation
         * conditional, or that adds a second slot, has to come here and
         * change this test on purpose rather than silently altering what
         * every deferring server depends on. Servers that defer must own an
         * endpoint and thread no other traffic reaches.
         */
        syscall::install_reply(server, second_caller);
        if (!server.reply.valid || server.reply.caller != second_caller.id ||
            server.reply.generation != second_caller.object.generation)
            return error_t::invalid_argument;
        if (server.reply.nonce == first_nonce)
            return error_t::invalid_argument; // nonces must not repeat

        /*
         * A notification is a SET of badge bits, not a counter. Signalling
         * the same badge twice is indistinguishable from signalling it once,
         * so every protocol built on notifications has to be edge-tolerant.
         * Consuming is destructive and returns everything at once.
         */
        static notification::notification value{};
        value.pending_badges = 0U;
        constexpr u64 badge_a = 1ULL << 3U;
        constexpr u64 badge_b = 1ULL << 9U;

        notification::signal(value, badge_a);
        notification::signal(value, badge_a);
        if (value.pending_badges != badge_a)
            return error_t::invalid_argument; // collapsed, not counted

        notification::signal(value, badge_b);
        if (value.pending_badges != (badge_a | badge_b))
            return error_t::invalid_argument; // accumulated as a set

        const u64 consumed = notification::consume(value);
        if (consumed != (badge_a | badge_b) || value.pending_badges != 0U)
            return error_t::invalid_argument;
        if (notification::consume(value) != 0U)
            return error_t::invalid_argument; // consumption is destructive

        pr_info("[TEST] name=ipc_reply_authority result=PASS one_slot=1 overwrites=1 "
                "nonce_nonzero=1 generation_checked=1\n");
        pr_info("[TEST] name=ipc_notification_badge_set result=PASS collapses=1 accumulates=1 "
                "destructive_consume=1\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::ipc
