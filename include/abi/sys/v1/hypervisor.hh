#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class hypervisor_operation : word_t
    {
        vm_reset = 0U,
        stage2_map = 1U,
        stage2_unmap = 2U,
        vcpu_configure = 3U,
        vcpu_run = 4U,
        vcpu_suspend = 5U,
        virtual_irq_inject = 6U,
        diagnostics = 7U,
        fuzz = 8U,
    };

    enum class vm_exit_reason : word_t
    {
        none = 0U,
        hypercall = 1U,
        wait = 2U,
        stage2_fault = 3U,
        system_register = 4U,
        virtual_timer = 5U,
        shutdown = 6U,
        unexpected = 7U,
    };

    enum class guest_hypercall : word_t
    {
        console_write = 1U,
        time_query = 2U,
        irq_acknowledge = 3U,
        shutdown = 4U,
    };
}
