#include <sys/kernel/kernel.hh>

extern "C" [[noreturn]] void sys_kernel_entry() noexcept
{
    sys::kernel::start();
}

extern "C" [[noreturn]] void sys_kernel_secondary_entry() noexcept
{
    sys::kernel::start_secondary();
}
