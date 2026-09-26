#pragma once

#include <sys/kernel/ipc/endpoint.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>

namespace sys::kernel::tests::lifecycle_race
{
    /*
     * Instruction-level race fuzz of the endpoint lifecycle (TST-018).
     *
     * `ipc_lifecycle_races` walks a scripted sequence -- cancel, timer
     * expiry, blocked destroy, server exit holding reply authority, endpoint
     * reuse across CPUs -- and each scenario reaches its interesting state
     * deliberately. That proves the transitions are right. It cannot reach
     * the interleavings, because a scripted test chooses when each step
     * happens and the bugs here are the ones that only occur when nobody
     * chose.
     *
     * Two properties are worth the harness. The first is the queue
     * arithmetic: sender_head, sender_tail and sender_count are three
     * variables that must stay in agreement through wraparound, and they are
     * consistent only for as long as the endpoint lock actually serialises
     * them -- exactly the sort of thing that survives every scripted test and
     * then fails under load. This kernel has already shipped one lock whose
     * fairness bug only appeared with real contention, so "the lock works"
     * is not a free assumption.
     *
     * The second is reuse. Retiring an endpoint and re-initialising it is
     * only safe if no sender slips in after `retiring` is observed. The
     * workers honour that flag the way real callers do, so a validate()
     * failure means the window genuinely leaked rather than the test racing
     * itself.
     */
    inline constexpr u32 race_cpu_count = 4U;
    inline constexpr u32 iterations_per_cpu = 128U;

    inline ::sys::kernel::ipc::endpoint contended{};
    /*
     * Distinct registered thread objects, one per worker, because validate()
     * resolves every queued reference and rejects duplicates.
     *
     * user_threads[1..3] cannot be used: CONFIG_ROOT_ONLY_BOOT sets
     * active_user_thread_count to 1, so only thread 0 is ever registered and
     * the rest resolve to nullptr -- the first run failed validate exactly
     * once per enqueue for that reason. Sharing thread 0 across all three
     * workers is no good either, since then only one of them could ever be
     * queued and the race would evaporate. Bare headers rather than whole
     * thread structs: the queue stores references, and nothing here needs a
     * thread's state or its address space.
     */
    inline object::header_t race_senders[race_cpu_count]{};
    inline object::reference_t race_references[race_cpu_count]{};
    inline volatile bool active{};
    inline volatile u32 claimed_mask{};
    inline volatile u32 done_mask{};
    inline volatile u64 enqueues[race_cpu_count]{};
    inline volatile u64 dequeues[race_cpu_count]{};
    inline volatile u64 anomalies[race_cpu_count]{};
    inline volatile u32 deepest_seen[race_cpu_count]{};
    inline volatile u64 wraps_seen[race_cpu_count]{};

    /*
     * A thread already blocked on send is not enqueued a second time, and
     * validate() enforces that by rejecting duplicates. The workers reuse
     * one reference each, so without this check a CPU winning two coin
     * flips in a row would queue itself twice and fail the invariant by the
     * test's own doing -- which is exactly what the first run did, 230
     * times. Checked under the endpoint lock, where the real precondition
     * also holds.
     */
    [[nodiscard]] inline bool already_queued(const ::sys::kernel::ipc::endpoint& value,
                                             const object::reference_t& reference) noexcept {
        for (u32 offset = 0U; offset < value.sender_count; ++offset) {
            const u32 index =
                (value.sender_head + offset) % ::sys::kernel::ipc::endpoint_capacity;
            if (value.senders[index].id == reference.id &&
                value.senders[index].generation == reference.generation)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline u32 xorshift(u32& state) noexcept {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        return state;
    }

    inline void service_job(cpu_id_t cpu) noexcept {
        if (!__atomic_load_n(&active, __ATOMIC_ACQUIRE) || cpu >= race_cpu_count)
            return;
        const u32 bit = 1U << cpu;
        if ((__atomic_fetch_or(&claimed_mask, bit, __ATOMIC_ACQ_REL) & bit) != 0U)
            return;

        /*
         * A real registered thread object, because validate() resolves every
         * queued reference -- a synthetic one would fail the invariant for
         * reasons that have nothing to do with the race.
         */
        const object::reference_t self = race_references[cpu];
        u32 state = 0x9e3779b9U ^ (cpu * 0x85ebca6bU);
        u64 local_enqueues = 0U;
        u64 local_dequeues = 0U;
        u64 local_anomalies = 0U;
        u32 local_deepest = 0U;
        u64 local_wraps = 0U;

        for (u32 index = 0U; index < iterations_per_cpu; ++index) {
            ::sys::kernel::ipc::lock(contended);
            /*
             * Honour `retiring` exactly as a real sender must. Enqueueing
             * through it would break the invariant by the test's own doing
             * and prove nothing about the kernel.
             */
            if (!contended.retiring && (xorshift(state) & 1U) != 0U &&
                !already_queued(contended, self)) {
                if (::sys::kernel::ipc::enqueue_sender(contended, self))
                    ++local_enqueues;
            } else {
                object::reference_t drained{};
                if (::sys::kernel::ipc::dequeue_sender(contended, drained))
                    ++local_dequeues;
            }
            /*
             * Sampled here rather than by the observer on CPU 0: the workers
             * are the ones mutating the ring, and CPU 0 gets far fewer turns
             * than they do, so sampling there reported a maximum depth of 1
             * and no wraparound at all for a run that plainly had both.
             */
            if (contended.sender_count > local_deepest)
                local_deepest = contended.sender_count;
            if (contended.sender_tail < contended.sender_head)
                ++local_wraps;
            if (!::sys::kernel::ipc::validate(contended))
                ++local_anomalies;
            ::sys::kernel::ipc::unlock(contended);
        }

        __atomic_store_n(&deepest_seen[cpu], local_deepest, __ATOMIC_RELAXED);
        __atomic_store_n(&wraps_seen[cpu], local_wraps, __ATOMIC_RELAXED);
        __atomic_store_n(&enqueues[cpu], local_enqueues, __ATOMIC_RELAXED);
        __atomic_store_n(&dequeues[cpu], local_dequeues, __ATOMIC_RELAXED);
        __atomic_store_n(&anomalies[cpu], local_anomalies, __ATOMIC_RELAXED);
        __atomic_fetch_or(&done_mask, bit, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline error_t run() noexcept
    {
        for (u32 cpu = 1U; cpu < race_cpu_count; ++cpu) {
            if (object::register_dynamic_object(race_senders[cpu], object::type_t::thread) !=
                error_t::success)
                return error_t::invalid_argument;
            race_references[cpu] = object::reference(race_senders[cpu]);
        }

        ::sys::kernel::ipc::initialize(contended);
        contended.allocated = 1U;

        __atomic_store_n(&claimed_mask, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&done_mask, 0U, __ATOMIC_RELEASE);
        for (u32 cpu = 0U; cpu < race_cpu_count; ++cpu) {
            __atomic_store_n(&enqueues[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&dequeues[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&anomalies[cpu], 0U, __ATOMIC_RELAXED);
        }
        __atomic_store_n(&active, true, __ATOMIC_RELEASE);

        for (cpu_id_t cpu = 1U; cpu < race_cpu_count; ++cpu)
            platform::interrupt::send_ipi(cpu, platform::interrupt::reschedule_ipi);

        /*
         * CPU 0 drives the retire/reuse cycle against live senders: drain,
         * mark retiring, re-initialise. That is the sequence endpoint reuse
         * performs, and the window it opens -- between marking and
         * re-initialising -- is the one a late sender would fall into.
         */
        constexpr u32 worker_mask = ((1U << race_cpu_count) - 1U) & ~1U;
        u64 reuse_cycles = 0U;
        u32 deepest = 0U;
        u64 wraparounds = 0U;
        u64 revoker_anomalies = 0U;
        u32 spins = 0U;
        while (__atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) != worker_mask &&
               spins++ < 2000000U) {
            /*
             * Retire only occasionally, and just observe the rest of the
             * time. Retiring on every pass drains the queue faster than
             * three workers can fill it -- the first run managed 89
             * enqueues against 1500 reuse cycles, so the queue was almost
             * always empty and the head/tail wraparound this is meant to
             * stress never happened. Leaving the endpoint alone between
             * cycles lets senders accumulate and wrap before the next
             * teardown arrives.
             */
            const bool retire = (spins & 0x7U) == 0U;

            ::sys::kernel::ipc::lock(contended);
            if (retire) {
                object::reference_t drained{};
                while (::sys::kernel::ipc::dequeue_sender(contended, drained)) {
                }
                contended.retiring = true;
            }
            if (!::sys::kernel::ipc::validate(contended))
                ++revoker_anomalies;
            ::sys::kernel::ipc::unlock(contended);

            if (!retire)
                continue;

            ::sys::kernel::ipc::lock(contended);
            ::sys::kernel::ipc::initialize(contended);
            if (!::sys::kernel::ipc::validate(contended))
                ++revoker_anomalies;
            ::sys::kernel::ipc::unlock(contended);
            ++reuse_cycles;
        }
        __atomic_store_n(&active, false, __ATOMIC_RELEASE);

        const bool completed = __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) == worker_mask;

        u64 total_enqueues = 0U;
        u64 total_dequeues = 0U;
        u64 total_anomalies = revoker_anomalies;
        for (u32 cpu = 1U; cpu < race_cpu_count; ++cpu) {
            total_enqueues += __atomic_load_n(&enqueues[cpu], __ATOMIC_RELAXED);
            total_dequeues += __atomic_load_n(&dequeues[cpu], __ATOMIC_RELAXED);
            total_anomalies += __atomic_load_n(&anomalies[cpu], __ATOMIC_RELAXED);
            wraparounds += __atomic_load_n(&wraps_seen[cpu], __ATOMIC_RELAXED);
            if (__atomic_load_n(&deepest_seen[cpu], __ATOMIC_RELAXED) > deepest)
                deepest = __atomic_load_n(&deepest_seen[cpu], __ATOMIC_RELAXED);
        }

        /*
         * Final accounting, by hand rather than by trusting sender_count:
         * the counter and the slots drifting apart is precisely the
         * corruption a lost wraparound produces, and it is invisible to any
         * check that reads only one of them.
         */
        ::sys::kernel::ipc::lock(contended);
        u32 occupied = 0U;
        for (const auto& sender : contended.senders) {
            if (sender.type != object::type_t::none)
                ++occupied;
        }
        const u32 counted = contended.sender_count;
        const bool final_valid = ::sys::kernel::ipc::validate(contended);
        ::sys::kernel::ipc::unlock(contended);

        // Give the object table back what the test borrowed, before any
        // failure return: a leaked registration would fail an unrelated
        // lifetime invariant later and send the next reader chasing it.
        for (u32 cpu = 1U; cpu < race_cpu_count; ++cpu) {
            (void)object::unregister_object(race_references[cpu]);
            race_references[cpu] = {};
        }

        const bool consistent = occupied == counted;
        if (!completed || total_anomalies != 0U || !consistent || !final_valid) {
            pr_err("[TEST] name=ipc_lifecycle_race_fuzz result=FAIL completed=%u enqueues=%llu "
                   "dequeues=%llu reuse_cycles=%llu anomalies=%llu occupied=%u counted=%u "
                   "final_valid=%u done_mask=%x\n",
                   completed ? 1U : 0U, static_cast<unsigned long long>(total_enqueues),
                   static_cast<unsigned long long>(total_dequeues),
                   static_cast<unsigned long long>(reuse_cycles),
                   static_cast<unsigned long long>(total_anomalies), occupied, counted,
                   final_valid ? 1U : 0U, __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE));
            return error_t::invalid_argument;
        }

        /*
         * The race has to have actually happened. Zero enqueues would mean
         * every worker lost every coin flip or never ran at all, and the
         * test would be reporting a clean invariant it never stressed.
         */
        if (total_enqueues == 0U || reuse_cycles == 0U)
            return error_t::invalid_argument;

        pr_info("[TEST] name=ipc_lifecycle_race_fuzz result=PASS cpus=%u enqueues=%llu "
                "dequeues=%llu reuse_cycles=%llu deepest=%u wraparounds=%llu anomalies=0 "
                "queue_consistent=1 retire_respected=1\n",
                race_cpu_count - 1U, static_cast<unsigned long long>(total_enqueues),
                static_cast<unsigned long long>(total_dequeues),
                static_cast<unsigned long long>(reuse_cycles), deepest,
                static_cast<unsigned long long>(wraparounds));
        return error_t::success;
    }
} // namespace sys::kernel::tests::lifecycle_race
