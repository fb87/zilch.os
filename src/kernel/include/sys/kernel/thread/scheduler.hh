#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/entry.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::thread
{
    inline constexpr u32 user_thread_count = 10U;
    inline constexpr u32 maximum_cpu_count = 4U;
    inline thread user_threads[user_thread_count]{};
    inline u32 current_user_thread[maximum_cpu_count]{};
    inline bool user_execution_active[maximum_cpu_count]{};
    inline bool user_cpu_idle[maximum_cpu_count]{};
    inline volatile bool user_scheduler_ready = false;
    extern "C" [[noreturn]] void sys_kernel_user_idle() noexcept;
    inline volatile u64 per_cpu_switches[maximum_cpu_count]{};

    inline void initialize_user_threads() noexcept
    {
        for (u32 id = 0U; id < user_thread_count; ++id) {
            initialize_user(user_threads[id], static_cast<thread_id_t>(id),
                            static_cast<cpu_id_t>(id % maximum_cpu_count),
                            static_cast<word_t>(id),
                            static_cast<word_t>(initial_fuzz_seed(id)));
        }
        for (u32 cpu = 0U; cpu < maximum_cpu_count; ++cpu) {
            current_user_thread[cpu] = cpu;
            user_execution_active[cpu] = false;
            user_cpu_idle[cpu] = false;
            per_cpu_switches[cpu] = 0U;
        }
    }

    inline void launch_user_scheduler() noexcept
    {
        __atomic_store_n(&user_scheduler_ready, true, __ATOMIC_RELEASE);
        __asm__ volatile("sev" ::: "memory");
    }

    inline void wait_until_ready() noexcept
    {
        while (!__atomic_load_n(&user_scheduler_ready, __ATOMIC_ACQUIRE)) {
            __asm__ volatile("wfe" ::: "memory");
        }
    }

    [[nodiscard]] inline u32 current_index() noexcept
    {
        return current_user_thread[arch::cpu::current_id()];
    }

    [[nodiscard]] inline thread& current() noexcept
    {
        return user_threads[current_index()];
    }

    inline void save_current_user(const arch::thread::context& frame) noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (user_execution_active[cpu]) {
            arch::thread::copy(user_threads[current_user_thread[cpu]].context,
                               frame);
        }
    }

    [[nodiscard]] inline u32 next_runnable(cpu_id_t cpu, u32 after) noexcept
    {
        for (u32 offset = 1U; offset <= user_thread_count; ++offset) {
            const u32 candidate = (after + offset) % user_thread_count;
            if (user_threads[candidate].pinned_cpu == cpu
                && runnable(user_threads[candidate])) {
                return candidate;
            }
        }
        return after;
    }

    [[nodiscard]] inline bool validate_user_context(thread& value) noexcept
    {
        if (arch::thread::valid_user(value.context)) return true;

        ++value.faults;
        store_state(value, state::faulted);
        pr_err("user context rejected thread=%llu cpu=%u pc=%llx sp=%llx status=%llx\n",
               static_cast<unsigned long long>(value.id),
               static_cast<unsigned int>(value.pinned_cpu),
               static_cast<unsigned long long>(value.context.instruction_pointer),
               static_cast<unsigned long long>(value.context.stack_pointer),
               static_cast<unsigned long long>(value.context.status));
        return false;
    }

    inline void load_user(arch::thread::context& frame, u32 id) noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        thread& old = user_threads[current_user_thread[cpu]];
        if (load_state(old) == state::running) store_state(old, state::ready);

        u32 candidate = id;
        for (u32 attempts = 0U; attempts < user_thread_count; ++attempts) {
            thread& value = user_threads[candidate];
            if (value.pinned_cpu == cpu && runnable(value)
                && validate_user_context(value)) {
                current_user_thread[cpu] = candidate;
                user_cpu_idle[cpu] = false;
                store_state(value, state::running);
                consume_pending(value);
                if (!validate_user_context(value)) {
                    candidate = next_runnable(cpu, candidate);
                    continue;
                }
                value.address_space.activate();
                arch::thread::copy(frame, value.context);
                __atomic_fetch_add(&per_cpu_switches[cpu], 1U, __ATOMIC_RELAXED);
                return;
            }
            candidate = next_runnable(cpu, candidate);
        }

        user_cpu_idle[cpu] = true;
        arch::thread::prepare_kernel_idle(
            frame, reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
    }

    inline void schedule_user(arch::thread::context& frame) noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu]) return;

        /* This path is valid only when the IRQ interrupted user mode. */
        const u32 old = current_user_thread[cpu];
        save_current_user(frame);
        const u32 next = next_runnable(cpu, old);
        if (next != old || !runnable(user_threads[old])) load_user(frame, next);
    }

    [[nodiscard]] inline bool is_kernel_idle_frame(
        const arch::thread::context& frame) noexcept
    {
        return frame.instruction_pointer
                   == reinterpret_cast<uintptr_t>(&sys_kernel_user_idle)
            && (frame.status & 0xfU) == 0x5U;
    }

    [[nodiscard]] inline bool resume_user_from_idle(
        arch::thread::context& frame) noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu] || !user_cpu_idle[cpu]
            || !is_kernel_idle_frame(frame)) {
            return false;
        }

        /*
         * This is the only EL1 exception frame that may be replaced by an
         * EL0 context.  IRQs interrupting syscall, fault, printk, scheduler,
         * or other kernel execution must return to that kernel instruction.
         */
        const u32 old = current_user_thread[cpu];
        const u32 next = next_runnable(cpu, old);
        if (next != old || runnable(user_threads[next])) {
            load_user(frame, next);
            return !user_cpu_idle[cpu];
        }
        return false;
    }

    inline void prepare_block(arch::thread::context& frame,
                              state blocked_state) noexcept
    {
        thread& value = current();
        arch::thread::copy(value.context, frame);
        store_state(value, blocked_state);
    }

    inline void schedule_prepared(arch::thread::context& frame) noexcept
    {
        thread& value = current();
        const u32 next = next_runnable(value.pinned_cpu, current_index());
        if (next == current_index() && !runnable(user_threads[next])) {
            user_cpu_idle[value.pinned_cpu] = true;
            arch::thread::prepare_kernel_idle(
                frame, reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
            return;
        }
        load_user(frame, next);
    }

    inline void block_and_schedule(arch::thread::context& frame,
                                   state blocked_state) noexcept
    {
        prepare_block(frame, blocked_state);
        schedule_prepared(frame);
    }

    inline void wake(thread& value) noexcept
    {
        if (value.current_state != state::faulted
            && value.current_state != state::terminated) {
            store_state(value, state::ready);
        }
    }

    [[nodiscard]] inline bool handle_user_fault(arch::thread::context& frame,
                                                u64 vector, u64 syndrome,
                                                vaddr_t fault_address) noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu] || vector != 8U) return false;
        const u64 exception_class = (syndrome >> 26U) & 0x3fU;
        if (exception_class != 0x20U && exception_class != 0x21U
            && exception_class != 0x22U && exception_class != 0x24U
            && exception_class != 0x25U && exception_class != 0x26U) return false;

        thread& value = current();
        ++value.faults;
        store_state(value, state::faulted);
        pr_warn("user fault contained thread=%llu cpu=%u esr=%llx far=%llx pc=%llx\n",
                static_cast<unsigned long long>(value.id),
                static_cast<unsigned int>(cpu),
                static_cast<unsigned long long>(syndrome),
                static_cast<unsigned long long>(fault_address),
                static_cast<unsigned long long>(frame.instruction_pointer));
        const u32 next = next_runnable(cpu, current_index());
        if (next == current_index() && !runnable(user_threads[next])) {
            /*
             * The user fault has already been contained.  It is valid for the
             * remaining threads pinned to this CPU to be blocked in IPC.  In
             * that case, return to the per-CPU EL1 idle context and wait for a
             * timer or remote reschedule IPI to make a thread runnable.
             */
            user_cpu_idle[cpu] = true;
            arch::thread::prepare_kernel_idle(
                frame, reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
            return true;
        }
        load_user(frame, next);
        return true;
    }

    [[noreturn]] inline void enter_first_user_thread() noexcept
    {
        const cpu_id_t cpu = arch::cpu::current_id();
        arch::irq::disable();
        u32 first = cpu;
        if (!runnable(user_threads[first])) first = next_runnable(cpu, first);
        current_user_thread[cpu] = first;
        user_execution_active[cpu] = true;
        store_state(user_threads[first], state::running);
        consume_pending(user_threads[first]);
        user_threads[first].address_space.activate();
        arch::thread::enter_user(user_threads[first].context);
    }
} // namespace sys::kernel::thread
