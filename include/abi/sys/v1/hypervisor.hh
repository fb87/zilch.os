#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class hypervisor_operation : word_t {
        vm_reset = 0U,
        stage2_map = 1U,
        stage2_unmap = 2U,
        vcpu_configure = 3U,
        vcpu_run = 4U,
        vcpu_suspend = 5U,
        virtual_irq_inject = 6U,
        diagnostics = 7U,
        vcpu_resume = 8U,
        vcpu_stop = 9U,
        vm_create = 10U,
        vcpu_create = 11U,
        vcpu_destroy = 12U,
        vm_destroy = 13U,
        stage2_tracking_query = 14U,
        stage2_tracking_clear = 15U,
        vcpu_state_read = 16U,
        vcpu_state_write = 17U,
    };

    enum class vm_exit_reason : word_t {
        none = 0U,
        hypercall = 1U,
        wait = 2U,
        stage2_fault = 3U,
        system_register = 4U,
        virtual_timer = 5U,
        shutdown = 6U,
        unexpected = 7U,
        mmio = 8U,
    };

    enum class guest_hypercall : word_t {
        console_write = 1U,
        time_query = 2U,
        irq_acknowledge = 3U,
        shutdown = 4U,
        report = 5U,
        diagnostic = 6U,
    };

    struct vm_exit_result final {
        word_t status{};
        vm_exit_reason reason{vm_exit_reason::none};
        word_t syndrome{};
        word_t fault_address{};
        word_t guest_pc{};
        word_t qualification{};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return static_cast<word_t>(reason) <= static_cast<word_t>(vm_exit_reason::mmio);
        }
    };
} // namespace sys::abi::v1

sys::abi::v1::vm_exit_result sys_hypervisor_invoke_raw(sys::word_t operation, sys::word_t selector,
                                                       sys::word_t argument0, sys::word_t argument1,
                                                       sys::word_t argument2) noexcept;
