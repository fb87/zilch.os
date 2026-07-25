#pragma once

#include <abi/sys/v1/syscall_numbers.hh>
#include <sys/arch/cpu.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::syscall
{
    inline u64 total_fuzz_operations = 0U;
    inline u64 total_fuzz_failures = 0U;
    inline constexpr u64 fuzz_progress_interval = 4096U;

    [[nodiscard]] inline error_t decode_ipc_result(
        const thread::thread& current,
        const arch::thread::context& frame) noexcept
    {
        const word_t endpoint = arch::syscall::argument(frame, 0U);
        const word_t operation = arch::syscall::argument(frame, 1U);
        const word_t id = arch::syscall::argument(frame, 3U);

        if (endpoint != abi::v1::debug_endpoint
            && endpoint != abi::v1::fuzz_endpoint) {
            return error_t::denied;
        }
        if (operation != static_cast<word_t>(abi::v1::ipc_operation::call)) {
            return error_t::denied;
        }
        if (id != static_cast<word_t>(current.id)) {
            return error_t::invalid_argument;
        }
        return error_t::success;
    }

    inline void record_fuzz(thread::thread& current,
                            arch::thread::context& frame,
                            error_t result) noexcept
    {
        ++current.fuzz_iterations;
        ++total_fuzz_operations;

        error_t expected = error_t::unsupported;
        const auto test_case = static_cast<abi::v1::fuzz_case>(
            arch::syscall::argument(frame, 2U));
        switch (test_case) {
        case abi::v1::fuzz_case::valid_call:
        case abi::v1::fuzz_case::random_payload:
            expected = error_t::success;
            break;
        case abi::v1::fuzz_case::invalid_capability:
        case abi::v1::fuzz_case::invalid_operation:
        case abi::v1::fuzz_case::boundary_capability:
            expected = error_t::denied;
            break;
        case abi::v1::fuzz_case::wrong_thread_identity:
            expected = error_t::invalid_argument;
            break;
        case abi::v1::fuzz_case::mixed:
            expected = result;
            break;
        }

        if (result != expected || !thread::validate(current)) {
            ++current.fuzz_failures;
            ++total_fuzz_failures;
            pr_err("fuzz failure seed=%llx iteration=%llu thread=%llu case=%llu result=%d expected=%d\n",
                   static_cast<unsigned long long>(current.fuzz_seed),
                   static_cast<unsigned long long>(current.fuzz_iterations),
                   static_cast<unsigned long long>(current.id),
                   static_cast<unsigned long long>(arch::syscall::argument(frame, 2U)),
                   static_cast<int>(result), static_cast<int>(expected));
        }

        if ((total_fuzz_operations % fuzz_progress_interval) == 0U) {
            pr_info("fuzz progress operations=%llu failures=%llu thread=%llu seed=%llx iteration=%llu\n",
                    static_cast<unsigned long long>(total_fuzz_operations),
                    static_cast<unsigned long long>(total_fuzz_failures),
                    static_cast<unsigned long long>(current.id),
                    static_cast<unsigned long long>(current.fuzz_seed),
                    static_cast<unsigned long long>(current.fuzz_iterations));
        }
    }

    [[nodiscard]] inline bool dispatch_ipc(thread::thread& current,
                                           arch::thread::context& frame,
                                           u64 vector,
                                           u64 syndrome) noexcept
    {
        if (!arch::syscall::is_user_syscall(vector, syndrome)) {
            return false;
        }

        if (arch::syscall::number(frame)
            != static_cast<word_t>(abi::v1::syscall::ipc)) {
            arch::syscall::set_result(frame,
                static_cast<word_t>(static_cast<s64>(error_t::unsupported)));
            return true;
        }

        const error_t result = decode_ipc_result(current, frame);
        if (arch::syscall::argument(frame, 6U) == abi::v1::fuzz_magic) {
            record_fuzz(current, frame, result);
        } else if (result == error_t::success
                   && arch::syscall::argument(frame, 0U)
                       == abi::v1::debug_endpoint) {
            ++current.reports;
            pr_info("user thread=%llu cpu=%u counter=%llu ipc=call\n",
                    static_cast<unsigned long long>(current.id),
                    static_cast<unsigned int>(arch::cpu::current_id()),
                    static_cast<unsigned long long>(arch::syscall::argument(frame, 4U)));
        }

        arch::syscall::set_result(
            frame, static_cast<word_t>(static_cast<s64>(result)));
        return true;
    }
} // namespace sys::kernel::syscall
