#include <sys/kernel.hpp>

extern "C" [[noreturn]] void sys_kernel_entry() noexcept {
    sys::kernel::start();
}
