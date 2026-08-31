#pragma once

#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/guest_manifest.hh>
#include <sys/ipc.hh>
#include <sys/platform/v1/earlyfs.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

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
            if (control(abi::v1::control_operation::device_frame_create, frame, dev.ipa) != success)
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

    /*
     * Root-driven path lookup, wired up: resolves each PL3 server role's
     * binary by name in the earlyfs image (the same one the kernel embeds
     * and image_for_role() falls back to) and registers the result via
     * role_image_bind(), so the kernel/arch layer no longer needs to know
     * "role 0x203 means bin/domain-manager" for these roles -- it only
     * knows there's a (bounds-checked) span bound to that role number. Runs
     * once, before any process_create call, so every role is already bound
     * by the time image_for_role() is consulted for it.
     */
    inline constexpr capability_id_t earlyfs_frame_selector = 90U;
    /*
     * map_page() only accepts addresses in [user_code, user_stack_base) --
     * the stack page and anything at or past it are rejected outright, not
     * just "already occupied". This is one page below user_stack_base
     * (0x20040000), the same convention domain-manager's own scratch_address
     * (0x2003f000) uses: inside root's own 256 KiB image region, but past
     * where root's own (small) ELF segments actually land, so this L3 slot
     * is free for a one-off scratch mapping.
     */
    inline constexpr word_t earlyfs_scratch_address = 0x2003f000U;
    inline constexpr capability_id_t self_space_selector = 3U;

    struct role_image_entry final {
        word_t role;
        const char* name;
    };

    [[nodiscard]] inline bool bind_role_images() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        if (control(abi::v1::control_operation::earlyfs_frame_create, earlyfs_frame_selector, 0U) !=
            success)
            return false;
        const word_t read_only = static_cast<word_t>(abi::v1::CapabilityRight::read);
        // create_earlyfs_frame() marks the frame device=true (its physical
        // address is a fixed kernel-image address, not pool-allocator
        // backed -- same reason create_device_frame() sets it), and
        // memory::valid_attributes() requires memory_type::device with
        // outer/non-shareable for any frame with that flag, regardless of
        // what the underlying memory actually is.
        const word_t attrs = abi::v1::encode_mapping_attributes(
            abi::v1::memory_type::device, abi::v1::memory_shareability::non_shareable);
        if (control(abi::v1::control_operation::map_frame, self_space_selector,
                    earlyfs_frame_selector, earlyfs_scratch_address, read_only, attrs) != success)
            return false;

        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U; // one page: header + directory always fit here

        const role_image_entry bindings[] = {
            {memory_role, "bin/memory-server"},
            {static_cast<word_t>(abi::v1::control_plane_role::process), "bin/control-plane"},
            {static_cast<word_t>(abi::v1::control_plane_role::device), "bin/control-plane"},
            {static_cast<word_t>(abi::v1::control_plane_role::console), "bin/control-plane"},
            {static_cast<word_t>(abi::v1::control_plane_role::domain), "bin/domain-manager"},
            {static_cast<word_t>(abi::v1::control_plane_role::supervisor), "bin/control-plane"},
        };
        for (const auto& entry : bindings) {
            // find_span() only reads the directory (mapped here, one page) and
            // returns the entry's raw offset/size without dereferencing its
            // data -- the real binary data lives well beyond this one mapped
            // page. role_image_bind() independently re-validates the span
            // against the kernel's own known full image size.
            const auto found = platform::v1::earlyfs::find_span(page, directory_bound, entry.name);
            if (!found.found)
                return false;
            if (control(abi::v1::control_operation::role_image_bind, entry.role,
                        static_cast<word_t>(found.offset),
                        static_cast<word_t>(found.size)) != success)
                return false;
        }
        return true;
    }

    [[nodiscard]] inline int supervise() noexcept {
        if (!bind_role_images())
            return 1;
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
