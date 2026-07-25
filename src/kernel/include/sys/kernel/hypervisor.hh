#pragma once

#include <sys/arch/arch.hh>
#include <sys/kernel/object.hh>

namespace sys::kernel::hypervisor
{
    struct virtual_machine_t {
        object::header_t header;
        vm_id_t id;
        paddr_t stage_2_root;
    };

    struct virtual_cpu_t {
        object::header_t header;
        vcpu_id_t id;
        vm_id_t virtual_machine;
    };

    [[nodiscard]]
    inline error_t initialize() noexcept {
        if constexpr (!arch::hypervisor::available) {
            return error_t::unsupported;
        }
        return arch::hypervisor::initialize();
    }
} // namespace sys::kernel::hypervisor
