#include <sys/arch/stack.hh>
#include <sys/kernel/kernel.hh>

extern "C" [[noreturn]] void sys_kernel_entry(sys::uintptr_t firmware_data) noexcept {
    sys::arch::stack::initialize_current_cpu();
    sys::kernel::start(firmware_data);
}

extern "C" [[noreturn]] void sys_kernel_secondary_entry() noexcept {
    sys::arch::stack::initialize_current_cpu();
    sys::kernel::start_secondary();
}

extern "C" [[noreturn]] void sys_kernel_user_idle() noexcept {
    for (;;) {
        sys::arch::cpu::wait_for_event();
    }
}
