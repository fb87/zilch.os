#pragma once

#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduling/context.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>

namespace sys::kernel::tests::configure_race
{
    /*
     * Racing reconfiguration against a live thread across CPUs (TST-020).
     *
     * `scheduling_configure` rewrites priority, budget, period, affinity,
     * consumed and donated ticks, donation depth and replenishment state as
     * one lifecycle transition, and control.hh says plainly why it demands
     * the thread be suspended first: so a remote scheduler tick cannot
     * observe a partially rewritten context. `configure_fuzz` establishes
     * that the operation itself is all-or-nothing over 4096 generated
     * configurations. Neither shows the property holding when another CPU
     * is trying to change the thread's state at the same moment, which is
     * the only situation the suspension requirement exists for.
     *
     * Two things are raced here.
     *
     * The guard: CPU 0 attempts reconfiguration in a loop while the other
     * CPUs flip the thread between suspended and running underneath it. An
     * attempt that lands while the thread is running must be refused with
     * `busy` -- never partially applied, and never applied at all. The test
     * requires both outcomes to occur, because a run where every attempt
     * happened to find the thread suspended would prove nothing about the
     * guard.
     *
     * The rewrite: CPU 0 alternates between two configurations that differ
     * in every field, and the observers read the whole context back and
     * require it to be wholly one or wholly the other. A mixture is the
     * torn state the suspension requirement is meant to make unreachable.
     * Observers take the lifecycle lock, modelling a remote tick that
     * synchronises rather than one that does not -- an unsynchronised
     * reader would report tearing that no real caller can see, since a
     * suspended thread is on nobody's run queue.
     *
     * The victim is a standalone thread object on no run queue, so flipping
     * its state cannot disturb anything that is actually scheduled.
     */
    inline constexpr u32 race_cpu_count = 4U;
    inline constexpr u32 iterations_per_cpu = 128U;

    inline thread::thread victim{};
    inline volatile bool active{};
    inline volatile u32 claimed_mask{};
    inline volatile u32 done_mask{};
    inline volatile u64 observations[race_cpu_count]{};
    inline volatile u64 torn[race_cpu_count]{};
    inline volatile u64 flips[race_cpu_count]{};

    struct profile {
        u8 priority;
        u64 budget;
        u64 period;
        cpu_id_t affinity;
    };

    // Differ in every field, so a mixture cannot be mistaken for either.
    inline constexpr profile first{10U, 1000U, 4000U, 0U};
    inline constexpr profile second{20U, 2000U, 8000U, 1U};

    [[nodiscard]] inline bool matches(const scheduling::context& value,
                                      const profile& expected) noexcept {
        return value.priority == expected.priority &&
               value.effective_priority == expected.priority &&
               value.budget_ticks == expected.budget && value.period_ticks == expected.period &&
               value.affinity == expected.affinity;
    }

    inline void service_job(cpu_id_t cpu) noexcept {
        if (!__atomic_load_n(&active, __ATOMIC_ACQUIRE) || cpu >= race_cpu_count || cpu == 0U)
            return;
        const u32 bit = 1U << cpu;
        if ((__atomic_fetch_or(&claimed_mask, bit, __ATOMIC_ACQ_REL) & bit) != 0U)
            return;

        u64 local_observations = 0U;
        u64 local_torn = 0U;
        u64 local_flips = 0U;

        for (u32 index = 0U; index < iterations_per_cpu; ++index) {
            thread::lock_ipc_lifecycle();
            const scheduling::context& observed = victim.scheduling_context;
            if (!matches(observed, first) && !matches(observed, second))
                ++local_torn;
            ++local_observations;
            /*
             * Flip the state under the same lock the configure guard uses.
             * This is what makes the attempt on CPU 0 sometimes find a
             * running thread and have to refuse it.
             */
            thread::store_state(victim, (index & 1U) != 0U ? thread::state::running
                                                           : thread::state::suspended);
            ++local_flips;
            thread::unlock_ipc_lifecycle();
        }

        __atomic_store_n(&observations[cpu], local_observations, __ATOMIC_RELAXED);
        __atomic_store_n(&torn[cpu], local_torn, __ATOMIC_RELAXED);
        __atomic_store_n(&flips[cpu], local_flips, __ATOMIC_RELAXED);
        __atomic_fetch_or(&done_mask, bit, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline error_t run() noexcept
    {
        // Start from a known whole configuration, or the observers would
        // count the initial all-zero context as torn.
        thread::store_state(victim, thread::state::suspended);
        victim.reply.valid = false;
        /*
         * No `scheduling_context = {}` here: the aggregate assignment emits
         * a memcpy, which does not exist in a freestanding build. The static
         * starts zeroed and configure() below writes every field the
         * observers compare.
         */
        victim.scheduling_context.donation_depth = 0U;
        victim.scheduling_context.donated_ticks = 0U;
        if (scheduling::configure(victim.scheduling_context, first.priority, first.budget,
                                  first.period, first.affinity,
                                  platform::timer::ticks(0U)) != error_t::success)
            return error_t::invalid_argument;

        __atomic_store_n(&claimed_mask, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&done_mask, 0U, __ATOMIC_RELEASE);
        for (u32 cpu = 0U; cpu < race_cpu_count; ++cpu) {
            __atomic_store_n(&observations[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&torn[cpu], 0U, __ATOMIC_RELAXED);
            __atomic_store_n(&flips[cpu], 0U, __ATOMIC_RELAXED);
        }
        __atomic_store_n(&active, true, __ATOMIC_RELEASE);

        for (cpu_id_t cpu = 1U; cpu < race_cpu_count; ++cpu)
            platform::interrupt::send_ipi(cpu, platform::interrupt::reschedule_ipi);

        constexpr u32 worker_mask = ((1U << race_cpu_count) - 1U) & ~1U;
        u64 accepted = 0U;
        u64 refused = 0U;
        u64 applied_while_running = 0U;
        bool alternate = true;
        u32 spins = 0U;

        while (__atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) != worker_mask &&
               spins++ < 2000000U) {
            const profile& wanted = alternate ? second : first;

            /*
             * The same guard the syscall applies, for the same reason: this
             * is the code path under test, not a simplified stand-in.
             */
            thread::lock_ipc_lifecycle();
            const bool quiescent = thread::load_state(victim) == thread::state::suspended &&
                                   !victim.reply.valid &&
                                   victim.scheduling_context.donation_depth == 0U &&
                                   victim.scheduling_context.donated_ticks == 0U;
            if (quiescent) {
                if (scheduling::configure(victim.scheduling_context, wanted.priority,
                                          wanted.budget, wanted.period, wanted.affinity,
                                          platform::timer::ticks(0U)) == error_t::success) {
                    ++accepted;
                    alternate = !alternate;
                }
            } else {
                ++refused;
                /*
                 * A refusal must leave the context exactly as it was. If the
                 * guard ever let a rewrite start and then backed out, this
                 * is where the leftovers would show.
                 */
                if (!matches(victim.scheduling_context, first) &&
                    !matches(victim.scheduling_context, second))
                    ++applied_while_running;
            }
            thread::unlock_ipc_lifecycle();
        }
        __atomic_store_n(&active, false, __ATOMIC_RELEASE);

        const bool completed = __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE) == worker_mask;

        u64 total_observations = 0U;
        u64 total_torn = 0U;
        u64 total_flips = 0U;
        for (u32 cpu = 1U; cpu < race_cpu_count; ++cpu) {
            total_observations += __atomic_load_n(&observations[cpu], __ATOMIC_RELAXED);
            total_torn += __atomic_load_n(&torn[cpu], __ATOMIC_RELAXED);
            total_flips += __atomic_load_n(&flips[cpu], __ATOMIC_RELAXED);
        }

        /*
         * Both outcomes must have happened. All-accepted would mean the
         * observers never managed to leave the thread running, and
         * all-refused would mean the race never let a reconfiguration
         * through -- either way the run would be reporting a property it
         * did not exercise.
         */
        const bool exercised = accepted != 0U && refused != 0U && total_observations != 0U;

        if (!completed || total_torn != 0U || applied_while_running != 0U || !exercised) {
            pr_err("[TEST] name=scheduling_configure_race result=FAIL completed=%u accepted=%llu "
                   "refused=%llu observations=%llu torn=%llu applied_while_running=%llu "
                   "flips=%llu done_mask=%x\n",
                   completed ? 1U : 0U, static_cast<unsigned long long>(accepted),
                   static_cast<unsigned long long>(refused),
                   static_cast<unsigned long long>(total_observations),
                   static_cast<unsigned long long>(total_torn),
                   static_cast<unsigned long long>(applied_while_running),
                   static_cast<unsigned long long>(total_flips),
                   __atomic_load_n(&done_mask, __ATOMIC_ACQUIRE));
            return error_t::invalid_argument;
        }

        pr_info("[TEST] name=scheduling_configure_race result=PASS cpus=%u accepted=%llu "
                "refused=%llu observations=%llu torn=0 applied_while_running=0 flips=%llu\n",
                race_cpu_count - 1U, static_cast<unsigned long long>(accepted),
                static_cast<unsigned long long>(refused),
                static_cast<unsigned long long>(total_observations),
                static_cast<unsigned long long>(total_flips));
        return error_t::success;
    }
} // namespace sys::kernel::tests::configure_race
