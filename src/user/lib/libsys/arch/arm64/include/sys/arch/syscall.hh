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
        register word_t x0 asm("x0") = argument0;
        register word_t x1 asm("x1") = argument1;
        register word_t x2 asm("x2") = argument2;
        register word_t x3 asm("x3") = argument3;
        register word_t x4 asm("x4") = argument4;
        register word_t x5 asm("x5") = argument5;
        register word_t x8 asm("x8") = number;
        asm volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                     : "memory");
        return x0;
    }

    inline word_t ipc_invoke_raw(word_t endpoint, word_t operation, word_t message0,
                                 word_t message1, word_t message2, word_t message3,
                                 word_t transfer_descriptor, word_t timeout_descriptor) noexcept {
        register word_t x0 asm("x0") = endpoint;
        register word_t x1 asm("x1") = operation;
        register word_t x2 asm("x2") = message0;
        register word_t x3 asm("x3") = message1;
        register word_t x4 asm("x4") = message2;
        register word_t x5 asm("x5") = message3;
        register word_t x6 asm("x6") = transfer_descriptor;
        register word_t x7 asm("x7") = timeout_descriptor;
        register word_t x8 asm("x8") = static_cast<word_t>(abi::v1::syscall::ipc);
        asm volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x7), "r"(x8)
                     : "memory");
        return x0;
    }

    inline word_t invoke_result1_raw(word_t number, word_t argument0, word_t argument1,
                                     word_t argument2, word_t argument3, word_t argument4,
                                     word_t argument5, word_t* result1) noexcept {
        register word_t x0 asm("x0") = argument0;
        register word_t x1 asm("x1") = argument1;
        register word_t x2 asm("x2") = argument2;
        register word_t x3 asm("x3") = argument3;
        register word_t x4 asm("x4") = argument4;
        register word_t x5 asm("x5") = argument5;
        register word_t x8 asm("x8") = number;
        asm volatile("svc #0"
                     : "+r"(x0), "+r"(x1)
                     : "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                     : "memory");
        if (result1 != nullptr)
            *result1 = x1;
        return x0;
    }

    inline abi::v1::ipc_result ipc_exchange_raw(word_t endpoint, word_t operation, word_t message0,
                                                word_t message1, word_t message2, word_t message3,
                                                word_t transfer_descriptor,
                                                word_t timeout_descriptor) noexcept {
        register word_t x0 asm("x0") = endpoint;
        register word_t x1 asm("x1") = operation;
        register word_t x2 asm("x2") = message0;
        register word_t x3 asm("x3") = message1;
        register word_t x4 asm("x4") = message2;
        register word_t x5 asm("x5") = message3;
        register word_t x6 asm("x6") = transfer_descriptor;
        register word_t x7 asm("x7") = timeout_descriptor;
        register word_t x8 asm("x8") = static_cast<word_t>(abi::v1::syscall::ipc);
        asm volatile("svc #0"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
                     : "r"(x6), "r"(x7), "r"(x8)
                     : "memory");
        return {x0, x1, x2, x3, x4, x5};
    }

    inline abi::v1::vm_exit_result hypervisor_invoke_raw(word_t operation, word_t selector,
                                                         word_t argument0, word_t argument1,
                                                         word_t argument2) noexcept {
        register word_t x0 asm("x0") =
            static_cast<word_t>(abi::v1::control_operation::hypervisor_invoke);
        register word_t x1 asm("x1") = operation;
        register word_t x2 asm("x2") = selector;
        register word_t x3 asm("x3") = argument0;
        register word_t x4 asm("x4") = argument1;
        register word_t x5 asm("x5") = argument2;
        register word_t x8 asm("x8") = static_cast<word_t>(abi::v1::syscall::control);
        asm volatile("svc #0"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
                     : "r"(x8)
                     : "memory");
        return {x0, static_cast<abi::v1::vm_exit_reason>(x1), x2, x3, x4, x5};
    }
} // namespace sys::arch::syscall
