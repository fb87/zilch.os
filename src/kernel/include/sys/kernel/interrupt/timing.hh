#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/timer.hh>
#include <sys/types.hh>

namespace sys::kernel::interrupt::timing
{
    inline constexpr u32 maximum_cpus = 64U;
    inline constexpr u64 target_microseconds = 10000U;
    inline constexpr u64 latency_target_microseconds = 10000U;

    struct state {
        arch::irq::irq_state_t architecture{};
        u64 started_at{};
        cpu_id_t cpu{};
    };

    inline volatile u64 maximum_ticks[maximum_cpus]{};
    inline volatile u64 samples[maximum_cpus]{};

    [[nodiscard]] inline state save_and_disable() noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        const arch::irq::irq_state_t architecture = arch::irq::save_and_disable();
        return {architecture, arch::timer::counter(), cpu};
    }

    inline void restore(state value) noexcept {
        const u64 elapsed = arch::timer::counter() - value.started_at;
        if (value.cpu < maximum_cpus) {
            __atomic_fetch_add(&samples[value.cpu], 1U, __ATOMIC_RELAXED);
            u64 observed = __atomic_load_n(&maximum_ticks[value.cpu], __ATOMIC_RELAXED);
            while (elapsed > observed &&
                   !__atomic_compare_exchange_n(&maximum_ticks[value.cpu], &observed, elapsed,
                                                false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            }
        }
        arch::irq::restore(value.architecture);
    }

    [[nodiscard]] inline u64 maximum() noexcept {
        u64 result = 0U;
        for (u32 cpu = 0U; cpu < maximum_cpus; ++cpu) {
            const u64 observed = __atomic_load_n(&maximum_ticks[cpu], __ATOMIC_ACQUIRE);
            if (observed > result)
                result = observed;
        }
        return result;
    }

    [[nodiscard]] inline u64 sample_count() noexcept {
        u64 result = 0U;
        for (u32 cpu = 0U; cpu < maximum_cpus; ++cpu)
            result += __atomic_load_n(&samples[cpu], __ATOMIC_ACQUIRE);
        return result;
    }

    [[nodiscard]] inline u64 target_ticks() noexcept {
        const u64 frequency = arch::timer::frequency();
        return frequency == 0U ? ~0ULL : frequency / 1000000U * target_microseconds;
    }

    [[nodiscard]] inline bool within_target() noexcept {
        return sample_count() != 0U && maximum() <= target_ticks();
    }

    enum class latency_kind : u8 {
        interrupt_service,
        preemption_service,
        cross_cpu_wake,
        ipc_service,
        count,
    };

    inline constexpr u32 latency_kind_count = static_cast<u32>(latency_kind::count);
    inline volatile u64 latency_maximum[latency_kind_count][maximum_cpus]{};
    inline volatile u64 latency_samples[latency_kind_count][maximum_cpus]{};
    inline volatile u64 wake_started[maximum_cpus]{};

    inline void record(latency_kind kind, cpu_id_t cpu, u64 elapsed) noexcept {
        const u32 index = static_cast<u32>(kind);
        if (index >= latency_kind_count || cpu >= maximum_cpus)
            return;
        __atomic_fetch_add(&latency_samples[index][cpu], 1U, __ATOMIC_RELAXED);
        u64 observed = __atomic_load_n(&latency_maximum[index][cpu], __ATOMIC_RELAXED);
        while (elapsed > observed &&
               !__atomic_compare_exchange_n(&latency_maximum[index][cpu], &observed, elapsed, false,
                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        }
    }

    struct latency_scope {
        latency_kind kind{};
        cpu_id_t cpu{};
        u64 started_at{};

        explicit latency_scope(latency_kind value) noexcept
            : kind(value), cpu(arch::cpu::current_id()), started_at(arch::timer::counter()) {}

        ~latency_scope() noexcept {
            record(kind, cpu, arch::timer::counter() - started_at);
        }
    };

    inline void begin_cross_cpu_wake(cpu_id_t target) noexcept {
        if (target >= maximum_cpus || target == arch::cpu::current_id())
            return;
        u64 expected = 0U;
        const u64 started = arch::timer::counter();
        (void)__atomic_compare_exchange_n(&wake_started[target], &expected, started, false,
                                          __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    }

    inline void complete_cross_cpu_wake(cpu_id_t cpu) noexcept {
        if (cpu >= maximum_cpus)
            return;
        const u64 started = __atomic_exchange_n(&wake_started[cpu], 0U, __ATOMIC_ACQ_REL);
        if (started != 0U)
            record(latency_kind::cross_cpu_wake, cpu, arch::timer::counter() - started);
    }

    [[nodiscard]] inline u64 latency_max(latency_kind kind) noexcept {
        const u32 index = static_cast<u32>(kind);
        u64 result = 0U;
        if (index >= latency_kind_count)
            return result;
        for (u32 cpu = 0U; cpu < maximum_cpus; ++cpu) {
            const u64 observed = __atomic_load_n(&latency_maximum[index][cpu], __ATOMIC_ACQUIRE);
            if (observed > result)
                result = observed;
        }
        return result;
    }

    [[nodiscard]] inline u64 latency_sample_count(latency_kind kind) noexcept {
        const u32 index = static_cast<u32>(kind);
        u64 result = 0U;
        if (index >= latency_kind_count)
            return result;
        for (u32 cpu = 0U; cpu < maximum_cpus; ++cpu)
            result += __atomic_load_n(&latency_samples[index][cpu], __ATOMIC_ACQUIRE);
        return result;
    }

    [[nodiscard]] inline u64 latency_target_ticks() noexcept {
        const u64 frequency = arch::timer::frequency();
        return frequency == 0U ? ~0ULL : frequency / 1000000U * latency_target_microseconds;
    }

    [[nodiscard]] inline bool latency_within_target(latency_kind kind) noexcept {
        return latency_sample_count(kind) != 0U && latency_max(kind) <= latency_target_ticks();
    }
} // namespace sys::kernel::interrupt::timing
