#pragma once

#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduler.hh>
#include <sys/kernel/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::scheduler_test
{
    inline constexpr u32 worker_count = 10U;
    inline constexpr u32 report_count_per_worker = 3U;

    struct worker_argument_t {
        thread_id_t thread_id;
        cpu_id_t cpu;
        u32 delay;
        volatile u64 counter;
        volatile u32 reports;
    };

    inline thread::thread_t workers[worker_count]{};
    inline worker_argument_t arguments[worker_count]{};

    inline void worker_step(void* opaque) noexcept
    {
        auto* argument = static_cast<worker_argument_t*>(opaque);
        if (argument == nullptr) {
            return;
        }

        const u64 counter =
            __atomic_add_fetch(&argument->counter, 1U, __ATOMIC_RELAXED);
        if ((counter % argument->delay) != 0U) {
            return;
        }

        __atomic_fetch_add(&argument->reports, 1U, __ATOMIC_RELAXED);
        pr_info("thread id=%llu cpu=%u counter=%llu delay=%u\n",
                static_cast<unsigned long long>(argument->thread_id),
                static_cast<unsigned int>(arch::cpu::current_id()),
                static_cast<unsigned long long>(counter),
                static_cast<unsigned int>(argument->delay));
    }

    [[nodiscard]] inline bool initialize(u32 cpu_count) noexcept
    {
        if (cpu_count == 0U || cpu_count > scheduler::maximum_cpu_count) {
            return false;
        }

        for (u32 index = 0U; index < worker_count; ++index) {
            const cpu_id_t cpu = static_cast<cpu_id_t>(index % cpu_count);
            const thread_id_t thread_id = 0x2000U + index;
            const u32 delay = index + 1U;

            arguments[index] = {
                .thread_id = thread_id,
                .cpu = cpu,
                .delay = delay,
                .counter = 0U,
                .reports = 0U,
            };

            thread::initialize_kernel(workers[index],
                                      thread_id,
                                      cpu,
                                      worker_step,
                                      &arguments[index],
                                      1U);
            if (!scheduler::make_ready(workers[index])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline u64 steps(u32 worker) noexcept
    {
        if (worker >= worker_count) {
            return 0U;
        }
        return __atomic_load_n(&arguments[worker].counter, __ATOMIC_ACQUIRE);
    }

    [[nodiscard]] inline u32 reports(u32 worker) noexcept
    {
        if (worker >= worker_count) {
            return 0U;
        }
        return __atomic_load_n(&arguments[worker].reports, __ATOMIC_ACQUIRE);
    }

    [[nodiscard]] inline bool complete() noexcept
    {
        for (u32 worker = 0U; worker < worker_count; ++worker) {
            if (reports(worker) < report_count_per_worker) {
                return false;
            }
        }
        return true;
    }
} // namespace sys::kernel::scheduler_test
