#include <sys/kernel/kernel.hh>

extern "C" [[noreturn]] void sys_kernel_entry() noexcept {
    sys::kernel::start();
}

extern "C" [[noreturn]] void sys_kernel_secondary_entry() noexcept {
    sys::kernel::start_secondary();
}

extern "C" [[noreturn]] void sys_kernel_user_idle() noexcept {
    for (;;) {
        sys::arch::cpu::wait_for_event();
    }
}
