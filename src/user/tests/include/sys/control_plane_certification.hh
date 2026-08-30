#pragma once

#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>

namespace sys::control_plane_certification
{
    template <typename Create, typename Destroy, typename Wait, typename Probe>
    [[nodiscard]] bool run(Create create, Destroy destroy, Wait wait, Probe probe) noexcept {
        constexpr word_t selector_base = 40U;
        constexpr word_t selector_stride = 3U;
        constexpr word_t endpoint_base = 16U;
        constexpr word_t service_endpoint = 11U;
        constexpr word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
        const word_t first_role = static_cast<word_t>(abi::v1::control_plane_role::process);
        bool created[abi::v1::control_plane_role_count]{};
        bool endpoints[abi::v1::control_plane_role_count]{};
        bool passed = true;

        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index) {
            const word_t selector = selector_base + index * selector_stride;
            endpoints[index] =
                control(abi::v1::control_operation::endpoint_create, endpoint_base + index) ==
                static_cast<word_t>(error_t::success);
            created[index] = endpoints[index] && create(index % 4U, first_role + index, selector,
                                                        selector + 1U, selector + 2U);
            if (created[index])
                created[index] = control(abi::v1::control_operation::capability_mint, selector + 1U,
                                         service_endpoint, endpoint_base + index, read_write,
                                         index + 1U) == static_cast<word_t>(error_t::success);
            passed = created[index] && passed;
        }
        if (passed)
            passed = wait((1U << abi::v1::control_plane_role_count) - 1U, nullptr);
        for (word_t index = 0U; passed && index < abi::v1::control_plane_role_count; ++index) {
            const auto reply =
                ipc_call(endpoint_base + index,
                         static_cast<word_t>(abi::v1::control_plane_operation::health), 0U);
            passed = reply.status == static_cast<word_t>(error_t::success) &&
                     reply.message0 == abi::v1::control_plane_health_magic &&
                     reply.message1 == first_role + index;
        }
        for (word_t index = 0U; passed && index < abi::v1::control_plane_role_count; ++index)
            passed = probe(index, endpoint_base + index) && passed;

        constexpr word_t restart_index = 0U;
        const word_t restart_role = first_role + restart_index;
        const auto restart_policy = control_plane::policy_for(restart_role);
        if (passed)
            passed = control_plane::may_restart(restart_policy, 0U);
        if (passed) {
            const auto stopped =
                ipc_call(endpoint_base + restart_index,
                         static_cast<word_t>(abi::v1::control_plane_operation::stop), 0U);
            passed = stopped.status == static_cast<word_t>(error_t::success) &&
                     stopped.message0 == 0U && stopped.message1 == restart_role;
        }
        if (passed)
            passed = wait(abi::v1::control_plane_exit_badge(restart_role), nullptr);
        if (passed) {
            const word_t selector = selector_base + restart_index * selector_stride;
            passed =
                destroy(selector, selector + 1U, selector + 2U, nullptr) &&
                create(restart_index % 4U, restart_role, selector, selector + 1U, selector + 2U);
            if (passed)
                passed = control(abi::v1::control_operation::capability_mint, selector + 1U,
                                 service_endpoint, endpoint_base + restart_index, read_write,
                                 restart_index + 1U) == static_cast<word_t>(error_t::success);
        }
        if (passed)
            passed = wait(abi::v1::control_plane_ready_badge(restart_role), nullptr);
        if (passed) {
            const auto recovered =
                ipc_call(endpoint_base + restart_index,
                         static_cast<word_t>(abi::v1::control_plane_operation::health), 0U);
            passed = recovered.status == static_cast<word_t>(error_t::success) &&
                     recovered.message0 == abi::v1::control_plane_health_magic &&
                     recovered.message1 == restart_role;
        }

        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index) {
            if (created[index]) {
                const word_t selector = selector_base + index * selector_stride;
                passed = destroy(selector, selector + 1U, selector + 2U, nullptr) && passed;
            }
        }
        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index)
            if (endpoints[index])
                passed = control(abi::v1::control_operation::endpoint_destroy,
                                 endpoint_base + index) == static_cast<word_t>(error_t::success) &&
                         passed;
        return passed;
    }
} // namespace sys::control_plane_certification
