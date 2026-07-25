#include <sys/hypervisor/hypervisor.h>

namespace sys::hypervisor
{
    Error initialize() noexcept
    {
        return Error::unsupported;
    }

    Error create_vm(vm_id_t) noexcept
    {
        return Error::unsupported;
    }

    Error run(vm_id_t) noexcept
    {
        return Error::unsupported;
    }
} // namespace sys::hypervisor
