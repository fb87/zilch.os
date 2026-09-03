#pragma once

#include <sys/types.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>
#include <abi/sys/v1/ipc.hh>
#include <abi/sys/v1/syscall.hh>

namespace sys::arch::syscall
{
    inline word_t invoke_raw(word_t number, word_t argument0, word_t argument1, word_t argument2,
                             word_t argument3, word_t argument4, word_t argument5) noexcept {
        register word_t rax asm("rax") = number;
        register word_t rdi asm("rdi") = argument0;
        register word_t rsi asm("rsi") = argument1;
        register word_t rdx asm("rdx") = argument2;
        register word_t r10 asm("r10") = argument3;
        register word_t r8 asm("r8") = argument4;
        register word_t r9 asm("r9") = argument5;
        asm volatile("syscall"
                     : "+r"(rax)
                     : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
        return rax;
    }

    /* No dedicated fast-path IPC entry yet: out-of-line transfer/timeout
     * descriptors aren't supported on this architecture's syscall path, so
     * anything requesting them is honestly reported as unsupported rather
     * than silently dropped. The plain case reuses invoke_raw().
     */
    inline word_t ipc_invoke_raw(word_t endpoint, word_t operation, word_t message0,
                                 word_t message1, word_t message2, word_t message3,
                                 word_t transfer_descriptor, word_t timeout_descriptor) noexcept {
        if (transfer_descriptor != 0U || timeout_descriptor != 0U)
            return static_cast<word_t>(static_cast<s64>(error_t::unsupported));
        return invoke_raw(static_cast<word_t>(abi::v1::syscall::ipc), endpoint, operation, message0,
                          message1, message2, message3);
    }

    inline word_t invoke_result1_raw(word_t number, word_t argument0, word_t argument1,
                                     word_t argument2, word_t argument3, word_t argument4,
                                     word_t argument5, word_t* result1) noexcept {
        if (result1 != nullptr)
            *result1 = 0U;
        return invoke_raw(number, argument0, argument1, argument2, argument3, argument4, argument5);
    }

    inline abi::v1::ipc_result ipc_exchange_raw(word_t endpoint, word_t operation, word_t message0,
                                                word_t message1, word_t message2, word_t message3,
                                                word_t transfer_descriptor,
                                                word_t timeout_descriptor) noexcept {
        abi::v1::ipc_result result{};
        result.status = ipc_invoke_raw(endpoint, operation, message0, message1, message2, message3,
                                       transfer_descriptor, timeout_descriptor);
        return result;
    }

    inline abi::v1::vm_exit_result hypervisor_invoke_raw(word_t operation, word_t selector,
                                                         word_t argument0, word_t argument1,
                                                         word_t argument2) noexcept {
        abi::v1::vm_exit_result result{};
        result.status =
            invoke_raw(static_cast<word_t>(abi::v1::syscall::control),
                       static_cast<word_t>(abi::v1::control_operation::hypervisor_invoke),
                       operation, selector, argument0, argument1, argument2);
        return result;
    }
} // namespace sys::arch::syscall
