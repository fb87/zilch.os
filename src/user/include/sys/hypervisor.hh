#pragma once

#include <sys/control.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>

namespace sys
{
    struct stage2_tracking_result final {
        word_t status{};
        word_t flags{};
    };

    struct vcpu_state_result final {
        word_t status{};
        word_t value{};
    };
    [[nodiscard]] inline word_t hypervisor_invoke(abi::v1::hypervisor_operation operation,
                                                  capability_id_t selector, word_t argument0 = 0U,
                                                  word_t argument1 = 0U,
                                                  word_t argument2 = 0U) noexcept {
        return control(abi::v1::control_operation::hypervisor_invoke,
                       static_cast<word_t>(operation), selector, argument0, argument1, argument2);
    }

    [[nodiscard]] inline abi::v1::vm_exit_result vcpu_run(capability_id_t selector) noexcept {
        return sys_hypervisor_invoke_raw(
            static_cast<word_t>(abi::v1::hypervisor_operation::vcpu_run), selector, 0U, 0U, 0U);
    }

    [[nodiscard]] inline word_t vcpu_pause(capability_id_t selector) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_suspend, selector);
    }

    [[nodiscard]] inline word_t vcpu_resume(capability_id_t selector) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_resume, selector);
    }

    [[nodiscard]] inline word_t vcpu_stop(capability_id_t selector) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_stop, selector);
    }

    [[nodiscard]] inline word_t vm_create(capability_id_t destination,
                                          word_t counter_offset = 0U) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vm_create, destination,
                                 counter_offset);
    }

    [[nodiscard]] inline word_t vcpu_create(capability_id_t vm, capability_id_t destination,
                                            word_t logical_id) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_create, vm, destination,
                                 logical_id);
    }

    [[nodiscard]] inline word_t vcpu_destroy(capability_id_t selector) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_destroy, selector);
    }

    [[nodiscard]] inline word_t vm_destroy(capability_id_t selector) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vm_destroy, selector);
    }

    [[nodiscard]] inline stage2_tracking_result stage2_tracking(capability_id_t vm, word_t ipa,
                                                                bool clear = false) noexcept {
        const auto result = sys_hypervisor_invoke_raw(
            static_cast<word_t>(clear ? abi::v1::hypervisor_operation::stage2_tracking_clear
                                      : abi::v1::hypervisor_operation::stage2_tracking_query),
            vm, ipa, 0U, 0U);
        return {result.status, static_cast<word_t>(result.reason)};
    }

    [[nodiscard]] inline vcpu_state_result vcpu_state_read(capability_id_t vcpu,
                                                           word_t field) noexcept {
        const auto result = sys_hypervisor_invoke_raw(
            static_cast<word_t>(abi::v1::hypervisor_operation::vcpu_state_read), vcpu, field, 0U,
            0U);
        return {result.status, static_cast<word_t>(result.reason)};
    }

    [[nodiscard]] inline word_t vcpu_state_write(capability_id_t vcpu, word_t field,
                                                 word_t value) noexcept {
        return hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_state_write, vcpu, field,
                                 value);
    }
} // namespace sys
