#include <sys/types.hpp>

extern "C" [[noreturn]] void sys_user_exit(sys::s32 status) noexcept {
    (void)status;
    for (;;) {
#if defined(__aarch64__)
        asm volatile("wfe");
#elif defined(__x86_64__)
        asm volatile("pause");
#endif
    }
}
