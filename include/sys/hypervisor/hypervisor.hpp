#pragma once
#include <sys/types.hpp>

namespace sys::hypervisor
{
    [[nodiscard]] Error initialize() noexcept;
    [[nodiscard]] Error create_vm(vm_id_t id) noexcept;
    [[nodiscard]] Error run(vm_id_t id) noexcept;
} // namespace sys::hypervisor
