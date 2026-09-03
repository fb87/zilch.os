#pragma once

#include <sys/arch/cpu.hh>
#include <sys/control.hh>
#include <sys/syscall.hh>

#include <abi/sys/v1/thread.hh>

namespace sys
{
    [[noreturn]] inline void thread_exit(word_t status = 0U, capability_id_t notification = 0U,
                                         word_t badge = 0U) noexcept {
        (void)control(abi::v1::control_operation::thread_exit, status, notification, badge);
        for (;;) {
            arch::cpu::relax();
        }
    }
} // namespace sys
