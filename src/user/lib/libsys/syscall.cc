#include <abi/sys/v1/ipc.hh>
#include <abi/sys/v1/syscall.hh>

extern "C" sys::word_t sys_invoke_raw(sys::word_t number, sys::word_t argument0,
                                      sys::word_t argument1, sys::word_t argument2,
                                      sys::word_t argument3, sys::word_t argument4,
                                      sys::word_t argument5) noexcept {
#if defined(__aarch64__)
    register sys::word_t x0 asm("x0") = argument0;
    register sys::word_t x1 asm("x1") = argument1;
    register sys::word_t x2 asm("x2") = argument2;
    register sys::word_t x3 asm("x3") = argument3;
    register sys::word_t x4 asm("x4") = argument4;
    register sys::word_t x5 asm("x5") = argument5;
    register sys::word_t x8 asm("x8") = number;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                 : "memory");
    return x0;
#elif defined(__x86_64__)
    register sys::word_t rax asm("rax") = number;
    register sys::word_t rdi asm("rdi") = argument0;
    register sys::word_t rsi asm("rsi") = argument1;
    register sys::word_t rdx asm("rdx") = argument2;
    register sys::word_t r10 asm("r10") = argument3;
    register sys::word_t r8 asm("r8") = argument4;
    register sys::word_t r9 asm("r9") = argument5;
    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return rax;
#else
#error "Unsupported userspace architecture"
#endif
}

extern "C" sys::word_t sys_ipc_invoke_raw(sys::word_t endpoint, sys::word_t operation,
                                          sys::word_t message0, sys::word_t message1,
                                          sys::word_t message2, sys::word_t message3,
                                          sys::word_t transfer_descriptor,
                                          sys::word_t timeout_descriptor) noexcept {
#if defined(__aarch64__)
    register sys::word_t x0 asm("x0") = endpoint;
    register sys::word_t x1 asm("x1") = operation;
    register sys::word_t x2 asm("x2") = message0;
    register sys::word_t x3 asm("x3") = message1;
    register sys::word_t x4 asm("x4") = message2;
    register sys::word_t x5 asm("x5") = message3;
    register sys::word_t x6 asm("x6") = transfer_descriptor;
    register sys::word_t x7 asm("x7") = timeout_descriptor;
    register sys::word_t x8 asm("x8") = static_cast<sys::word_t>(sys::abi::v1::syscall::ipc);
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x7), "r"(x8)
                 : "memory");
    return x0;
#elif defined(__x86_64__)
    if (transfer_descriptor != 0U || timeout_descriptor != 0U) {
        return static_cast<sys::word_t>(static_cast<sys::s64>(sys::error_t::unsupported));
    }
    return sys_invoke_raw(static_cast<sys::word_t>(sys::abi::v1::syscall::ipc), endpoint, operation,
                          message0, message1, message2, message3);
#else
#error "Unsupported userspace architecture"
#endif
}

extern "C" sys::word_t sys_invoke_result1_raw(sys::word_t number, sys::word_t argument0,
                                              sys::word_t argument1, sys::word_t argument2,
                                              sys::word_t argument3, sys::word_t argument4,
                                              sys::word_t argument5,
                                              sys::word_t* result1) noexcept {
#if defined(__aarch64__)
    register sys::word_t x0 asm("x0") = argument0;
    register sys::word_t x1 asm("x1") = argument1;
    register sys::word_t x2 asm("x2") = argument2;
    register sys::word_t x3 asm("x3") = argument3;
    register sys::word_t x4 asm("x4") = argument4;
    register sys::word_t x5 asm("x5") = argument5;
    register sys::word_t x8 asm("x8") = number;
    asm volatile("svc #0"
                 : "+r"(x0), "+r"(x1)
                 : "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                 : "memory");
    if (result1 != nullptr)
        *result1 = x1;
    return x0;
#elif defined(__x86_64__)
    if (result1 != nullptr)
        *result1 = 0U;
    return sys_invoke_raw(number, argument0, argument1, argument2, argument3, argument4, argument5);
#else
#error "Unsupported userspace architecture"
#endif
}

sys::abi::v1::ipc_result sys_ipc_exchange_raw(sys::word_t endpoint, sys::word_t operation,
                                              sys::word_t message0, sys::word_t message1,
                                              sys::word_t message2, sys::word_t message3,
                                              sys::word_t transfer_descriptor,
                                              sys::word_t timeout_descriptor) noexcept {
    sys::abi::v1::ipc_result result{};
#if defined(__aarch64__)
    register sys::word_t x0 asm("x0") = endpoint;
    register sys::word_t x1 asm("x1") = operation;
    register sys::word_t x2 asm("x2") = message0;
    register sys::word_t x3 asm("x3") = message1;
    register sys::word_t x4 asm("x4") = message2;
    register sys::word_t x5 asm("x5") = message3;
    register sys::word_t x6 asm("x6") = transfer_descriptor;
    register sys::word_t x7 asm("x7") = timeout_descriptor;
    register sys::word_t x8 asm("x8") = static_cast<sys::word_t>(sys::abi::v1::syscall::ipc);
    asm volatile("svc #0"
                 : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
                 : "r"(x6), "r"(x7), "r"(x8)
                 : "memory");
    result = {x0, x1, x2, x3, x4, x5};
#else
    result.status = sys_ipc_invoke_raw(endpoint, operation, message0, message1, message2, message3,
                                       transfer_descriptor, timeout_descriptor);
#endif
    return result;
}
