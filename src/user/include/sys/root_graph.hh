#pragma once

#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/guest_manifest.hh>
#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>

namespace sys::root_graph
{
    inline constexpr word_t selector_base = 33U;
    inline constexpr word_t selector_stride = 3U;
    inline constexpr capability_id_t root_notification = 14U;
    inline constexpr word_t memory_role = 0x100U;
    inline constexpr word_t memory_selector =
        selector_base + abi::v1::control_plane_role_count * selector_stride;
    inline constexpr capability_id_t endpoint_base = memory_selector + 3U;
    inline constexpr capability_id_t service_endpoint = 11U;

    static_assert(endpoint_base + abi::v1::control_plane_role_count <= 64U);

    [[nodiscard]] inline bool launch(word_t index) noexcept {
        const word_t selector = selector_base + index * selector_stride;
        const word_t role = static_cast<word_t>(abi::v1::control_plane_role::process) + index;
        const capability_id_t endpoint = endpoint_base + index;
        constexpr word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
        if (control(abi::v1::control_operation::endpoint_create, endpoint) !=
            static_cast<word_t>(error_t::success))
            return false;
        if (control(abi::v1::control_operation::process_create, index % 4U, role, selector,
                    selector + 1U, selector + 2U) != static_cast<word_t>(error_t::success))
            return false;
        return control(abi::v1::control_operation::capability_mint, selector + 1U, service_endpoint,
                       endpoint, read_write, index + 1U) == static_cast<word_t>(error_t::success);
    }

    [[nodiscard]] inline bool healthy(word_t index) noexcept {
        const auto reply =
            ipc_call(endpoint_base + index,
                     static_cast<word_t>(abi::v1::control_plane_operation::health), 0U);
        const word_t role = static_cast<word_t>(abi::v1::control_plane_role::process) + index;
        return reply.status == static_cast<word_t>(error_t::success) &&
               reply.message0 == abi::v1::control_plane_health_magic && reply.message1 == role;
    }

#if CONFIG_GUEST_EMBEDDED_IMAGE
    /*
     * Root owns physical devices/interrupts and mints capabilities for them
     * into the domain-manager's cspace -- mirroring how it already creates
     * and mints the guest's stage-2 memory. Which devices and IRQs a guest
     * needs is read from its manifest, not hardcoded here: root has no idea
     * which guest it's booting.
     */
    [[nodiscard]] inline bool start_embedded_guest() noexcept {
        constexpr word_t domain_index = static_cast<word_t>(abi::v1::control_plane_role::domain) -
                                        static_cast<word_t>(abi::v1::control_plane_role::process);
        constexpr capability_id_t device_frame_base = 100U;
        constexpr capability_id_t device_irq_base = 116U;
        constexpr capability_id_t domain_device_frame_base = 100U;
        constexpr capability_id_t domain_device_irq_base = 116U;
        constexpr word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
        constexpr word_t write_control = static_cast<word_t>(abi::v1::CapabilityRight::write) |
                                         static_cast<word_t>(abi::v1::CapabilityRight::control);
        const capability_id_t domain_task = selector_base + domain_index * selector_stride + 1U;
        const capability_id_t domain_endpoint = endpoint_base + domain_index;
        const word_t success = static_cast<word_t>(error_t::success);

        const auto& manifest = ::sys_arm64_domain_guest_manifest;
        if (!guest_manifest::valid(manifest) ||
            manifest.device_count > guest_manifest::maximum_devices)
            return false;

        for (word_t index = 0U; index < manifest.device_count; ++index) {
            const auto& dev = manifest.devices[index];
            const capability_id_t frame = device_frame_base + index;
            const capability_id_t domain_frame = domain_device_frame_base + index;
            if (control(abi::v1::control_operation::device_frame_create, frame, dev.ipa) !=
                success)
                return false;
            if (control(abi::v1::control_operation::capability_mint, domain_task, domain_frame,
                        frame, read_write) != success)
                return false;
            if (dev.forward_irq == guest_manifest::no_irq)
                continue;
            const capability_id_t irq = device_irq_base + index;
            const capability_id_t domain_irq = domain_device_irq_base + index;
            if (control(abi::v1::control_operation::interrupt_create, irq, dev.forward_irq,
                        dev.forward_trigger) != success)
                return false;
            if (control(abi::v1::control_operation::capability_mint, domain_task, domain_irq, irq,
                        write_control) != success)
                return false;
        }
        if (ipc_call(domain_endpoint, static_cast<word_t>(abi::v1::control_plane_operation::launch),
                     0U)
                .status != success)
            return false;
        if (ipc_call(domain_endpoint, static_cast<word_t>(abi::v1::control_plane_operation::load),
                     0U)
                .status != success)
            return false;
        return ipc_call(domain_endpoint,
                        static_cast<word_t>(abi::v1::control_plane_operation::serve), 0U)
                   .status == success;
    }
#endif

    [[nodiscard]] inline int supervise() noexcept {
        if (control(abi::v1::control_operation::process_create, 1U, memory_role, memory_selector,
                    memory_selector + 1U,
                    memory_selector + 2U) != static_cast<word_t>(error_t::success))
            return 1;
        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index)
            if (!launch(index))
                return 1;

        const word_t expected =
            ((1U << abi::v1::control_plane_role_count) - 1U) | abi::v1::memory_service_ready_badge;
        word_t ready = 0U;
        for (;;) {
            word_t badges = 0U;
            const word_t status = control_result1(
                badges, abi::v1::control_operation::notification_poll, root_notification);
            if (status != static_cast<word_t>(error_t::success))
                return 2;
            if ((badges & (1U << 15U)) != 0U)
                return 3;
            ready |= badges;
            if ((ready & expected) == expected) {
                for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index)
                    if (!healthy(index))
                        return 4;
#if CONFIG_GUEST_EMBEDDED_IMAGE
                return start_embedded_guest() ? 0 : 5;
#endif
            }
        }
    }
} // namespace sys::root_graph
