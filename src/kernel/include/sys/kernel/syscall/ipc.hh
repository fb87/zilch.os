#pragma once

#include <abi/sys/v1/syscall_numbers.hh>
#include <sys/arch/cpu.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::syscall
{
    [[nodiscard]] inline bool dispatch_ipc(thread::thread& current,
                                           arch::thread::context& frame,
                                           u64 vector,
                                           u64 syndrome) noexcept
    {
        if (!arch::syscall::is_user_syscall(vector, syndrome)) {
            return false;
        }

        if (arch::syscall::number(frame) != static_cast<word_t>(abi::v1::syscall::ipc)) {
            arch::syscall::set_result(frame,
                static_cast<word_t>(static_cast<s64>(error_t::unsupported)));
            return true;
        }

        if (arch::syscall::argument(frame, 0U) != abi::v1::debug_endpoint
            || arch::syscall::argument(frame, 1U)
                != static_cast<word_t>(abi::v1::ipc_operation::call)) {
            arch::syscall::set_result(frame,
                static_cast<word_t>(static_cast<s64>(error_t::denied)));
            return true;
        }

        const u32 id = static_cast<u32>(arch::syscall::argument(frame, 3U));
        if (id != static_cast<u32>(current.id)) {
            arch::syscall::set_result(frame,
                static_cast<word_t>(static_cast<s64>(error_t::invalid_argument)));
            return true;
        }

        ++current.reports;
        pr_info("el0 task=%u cpu=%u counter=%llu delay=%llu ipc=call\n",
                static_cast<unsigned int>(id),
                static_cast<unsigned int>(arch::cpu::current_id()),
                static_cast<unsigned long long>(arch::syscall::argument(frame, 4U)),
                static_cast<unsigned long long>(arch::syscall::argument(frame, 5U)));
        arch::syscall::set_result(frame, static_cast<word_t>(error_t::success));
        return true;
    }
} // namespace sys::kernel::syscall
