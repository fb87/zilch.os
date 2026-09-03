#pragma once

/*
 * SMP IPC fuzz-certification bookkeeping -- exercised only by CONFIG_SELFTEST
 * builds' fuzz workers, never by production boot. Extracted out of
 * sys/kernel/syscall/ipc.hh (a production file), which now just calls
 * tests::certification::decode_fuzz_result()/record_fuzz() from its one
 * test-only branch in dispatch_ipc() before falling through to real IPC
 * operations.
 */

#include <sys/arch/cpu.hh>
#include <sys/arch/smp.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/kernel/verification/hooks.hh>
#include <sys/platform/timer.hh>
#include <sys/test_abi/v1/certification.hh>
#include <sys/types.hh>

#include <abi/sys/v1/ipc.hh>

namespace sys::kernel::tests::certification
{
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

    inline void log_fuzz_cpu_health(u64 operations, u64 rendezvous) noexcept {
        const u32 online = arch::smp::online_count();
        pr_info("smp fuzz total=%llu failures=%llu rendezvous=%llu cpus=%u\n",
                static_cast<unsigned long long>(operations),
                static_cast<unsigned long long>(
                    __atomic_load_n(&total_fuzz_failures, __ATOMIC_RELAXED)),
                static_cast<unsigned long long>(rendezvous), static_cast<unsigned int>(online));

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

    inline void record_fuzz(thread::thread& current, arch::thread::context& frame, error_t result,
                            u64 rendezvous) noexcept {
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
            log_fuzz_cpu_health(operations, rendezvous);
        }
    }
} // namespace sys::kernel::tests::certification
