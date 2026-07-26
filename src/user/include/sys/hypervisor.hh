#pragma once

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>
#include <sys/control.hh>
#include <sys/types.hh>

namespace sys
{
    [[nodiscard]] inline word_t hypervisor_invoke(
        abi::v1::hypervisor_operation operation, capability_id_t selector,
        word_t argument0 = 0U, word_t argument1 = 0U,
        word_t argument2 = 0U) noexcept
    {
        return control(abi::v1::control_operation::hypervisor_invoke,
                       static_cast<word_t>(operation), selector,
                       argument0, argument1, argument2);
    }
}
