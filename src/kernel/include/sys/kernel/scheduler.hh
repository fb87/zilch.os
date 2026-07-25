#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::scheduler
{
    inline constexpr u32 maximum_cpu_count = 4U;
    inline constexpr u32 maximum_threads_per_cpu = 8U;

    struct run_queue_t {
        thread::thread_t* entries[maximum_threads_per_cpu]{};
        u32 count{};
        u32 cursor{};
        thread::thread_t idle{};
        thread::thread_t* current{};
        volatile u64 schedule_count{};
        volatile u64 idle_count{};
    };

    inline run_queue_t run_queues[maximum_cpu_count]{};

    inline void idle_step(void*) noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu < maximum_cpu_count) {
            __atomic_fetch_add(&run_queues[cpu].idle_count, 1U, __ATOMIC_RELAXED);
        }
    }

    inline void initialize() noexcept
    {
        for (u32 cpu = 0U; cpu < maximum_cpu_count; ++cpu) {
            run_queues[cpu].count = 0U;
            run_queues[cpu].cursor = 0U;
            run_queues[cpu].current = nullptr;
            run_queues[cpu].schedule_count = 0U;
            run_queues[cpu].idle_count = 0U;
            for (u32 index = 0U; index < maximum_threads_per_cpu; ++index) {
                run_queues[cpu].entries[index] = nullptr;
            }
            thread::initialize_kernel(run_queues[cpu].idle,
                                      0x1000U + cpu,
                                      cpu,
                                      idle_step,
                                      nullptr,
                                      1U);
            run_queues[cpu].idle.state = thread::state_t::running;
            run_queues[cpu].current = &run_queues[cpu].idle;
        }
    }

    inline void initialize_cpu() noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu < maximum_cpu_count) {
            run_queues[cpu].current = &run_queues[cpu].idle;
            run_queues[cpu].idle.state = thread::state_t::running;
        }
    }

    [[nodiscard]] inline bool make_ready(thread::thread_t& target) noexcept
    {
        if (target.cpu >= maximum_cpu_count) {
            return false;
        }
        run_queue_t& queue = run_queues[target.cpu];
        if (queue.count >= maximum_threads_per_cpu) {
            return false;
        }
        target.state = thread::state_t::ready;
        queue.entries[queue.count++] = &target;
        return true;
    }

    inline void schedule_current_cpu() noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu >= maximum_cpu_count) {
            return;
        }

        run_queue_t& queue = run_queues[cpu];
        thread::thread_t* previous = queue.current;
        thread::thread_t* next = &queue.idle;

        if (queue.count != 0U) {
            queue.cursor = (queue.cursor + 1U) % queue.count;
            next = queue.entries[queue.cursor];
        }

        if (previous != nullptr && previous != &queue.idle
            && previous->state == thread::state_t::running) {
            previous->state = thread::state_t::ready;
        }
        next->state = thread::state_t::running;
        next->remaining_ticks = next->quantum_ticks;
        queue.current = next;
        __atomic_fetch_add(&queue.schedule_count, 1U, __ATOMIC_RELAXED);

        if (next->kernel_step != nullptr) {
            next->kernel_step(next->kernel_argument);
        }
    }

    inline void on_timer_tick() noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu >= maximum_cpu_count) {
            return;
        }
        thread::thread_t* current = run_queues[cpu].current;
        if (current == nullptr || current->remaining_ticks <= 1U) {
            schedule_current_cpu();
            return;
        }
        --current->remaining_ticks;
        if (current->kernel_step != nullptr) {
            current->kernel_step(current->kernel_argument);
        }
    }

    inline void on_reschedule_ipi() noexcept
    {
        schedule_current_cpu();
    }

    [[nodiscard]] inline u64 schedules(cpu_id_t cpu) noexcept
    {
        if (cpu >= maximum_cpu_count) {
            return 0U;
        }
        return __atomic_load_n(&run_queues[cpu].schedule_count, __ATOMIC_ACQUIRE);
    }
} // namespace sys::kernel::scheduler
