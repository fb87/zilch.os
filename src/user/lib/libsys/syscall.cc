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
