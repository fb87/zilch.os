#pragma once

#include <sys/console_client.hh>
#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/guest_manifest.hh>
#include <sys/ipc.hh>
#include <sys/platform/v1/earlyfs.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/fault.hh>
#include <abi/sys/v1/memory.hh>

namespace sys::root_graph
{
    inline constexpr word_t selector_base = 33U;
    inline constexpr word_t selector_stride = 3U;
    inline constexpr capability_id_t root_notification = 14U;
    /*
     * Every launched child's fault-endpoint capability (its own slot 10) is
     * minted from root's own slot 10 (create_user_bundle(),
     * scheduler.hh:518), so root receiving on its own slot 10 gets every
     * child's fault delivery, badged per-child via endpoint_badge(). Root
     * itself was also given this same slot 10 as its own fault endpoint at
     * boot (scheduler.hh's root bootstrap).
     */
    inline constexpr capability_id_t root_fault_endpoint = 10U;
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

    /*
     * device_frame_create() is root-gated, so the console-server can't
     * claim the UART itself -- root creates the one live device frame for
     * it (now exclusivity-checked, see memory::create_device_frame()) and
     * mints it into the console-server's cspace at the fixed slot its own
     * code expects (src/user/servers/console/main.cc's
     * uart_frame_selector), mirroring how start_embedded_guest() mints
     * device frames into the domain-manager. Capability slots are
     * per-cspace, so root's own scratch slot here doesn't collide with
     * anything in the console-server's cspace or vice versa.
     */
    inline constexpr word_t console_index = static_cast<word_t>(abi::v1::control_plane_role::console) -
                                            static_cast<word_t>(abi::v1::control_plane_role::process);
    inline constexpr capability_id_t console_uart_root_frame_selector = 95U;
    inline constexpr capability_id_t console_uart_child_frame_selector = 20U;
    inline constexpr word_t console_uart_physical_address = 0x09000000U;

    [[nodiscard]] inline bool mint_console_uart_frame() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const capability_id_t console_task = selector_base + console_index * selector_stride + 1U;
        if (control(abi::v1::control_operation::device_frame_create,
                    console_uart_root_frame_selector, console_uart_physical_address) != success)
            return false;
        return control(abi::v1::control_operation::capability_mint, console_task,
                       console_uart_child_frame_selector, console_uart_root_frame_selector,
                       read_write) == success;
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
        /*
         * The guest's UART is virtual (vPL011, see domain/main.cc) rather
         * than passed through -- the console-server (see mint_console_
         * uart_frame() above) owns the real hardware exclusively. The
         * domain-manager needs its own capability to call the
         * console-server's service endpoint directly (to forward TX bytes
         * and poll for RX) rather than going through root. This mints a
         * second capability to the exact same endpoint object root already
         * holds from the console-server's own launch() call -- no new
         * endpoint is created, matching how every other inter-service
         * capability in this codebase is derived from an
         * already-created source.
         */
        constexpr capability_id_t domain_console_endpoint_selector = 19U;
        constexpr word_t write_only = static_cast<word_t>(abi::v1::CapabilityRight::write);
        if (control(abi::v1::control_operation::capability_mint, domain_task,
                    domain_console_endpoint_selector, endpoint_base + console_index,
                    write_only) != success)
            return false;
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
            {static_cast<word_t>(abi::v1::control_plane_role::console), "bin/console-server"},
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

    /*
     * Path-based process launch: resolves an arbitrary earlyfs-resident
     * path at runtime (not just the six roles bind_role_images() resolves
     * once at boot) and launches it as a new process. Reuses the earlyfs
     * frame bind_role_images() already mapped at earlyfs_scratch_address --
     * that mapping is never torn down, so no new frame/map_frame calls are
     * needed here.
     *
     * dynamic_launch_role is a single reserved role slot (clear of every
     * role range used elsewhere: 0x100-0x117, 0x200-0x204). role_image_bind
     * upserts by matching role first, so calling this again with a
     * different path simply repoints this one role_bindings[] slot at the
     * new path -- this deliberately supports only one dynamically-launched
     * image resolvable at a time. Two different dynamically-launched
     * programs cannot both remain independently re-launchable
     * simultaneously without a second reserved role or an unbind
     * operation, neither of which exists today; that is a known scope
     * limit, not an oversight.
     */
    inline constexpr word_t dynamic_launch_role = 0x300U;

    /*
     * Root has exactly one thread at boot (kernel bootstrap gives it
     * initial_threads=1), and in the embedded-guest production
     * configuration, that one thread ends up permanently blocked inside
     * start_embedded_guest()'s call to the domain-manager's serve
     * operation -- which, per domain/main.cc's serve() loop, does not
     * return until the guest hits a genuinely terminal VM exit, i.e.
     * effectively never for a healthy guest. From that point on root was
     * not polling drain_fault_reports() at all: any fault in any other
     * service got zero application-level handling, falling back entirely
     * to the kernel's own 500-tick fault timeout.
     *
     * root_supervisor_role gives root a second thread (via thread_create,
     * which attaches a new thread to the CALLER's own existing task rather
     * than allocating a new one -- see create_user_thread() in
     * thread/scheduler.hh) that runs root's own binary (bin/init is
     * already earlyfs-addressable, see image.mk) under this reserved role
     * instead of root's normal bootstrap entry, and does nothing but drain
     * faults for the rest of the system's uptime. It shares root's cspace
     * (and thus its capabilities and root authority) automatically --
     * thread_create needs no minting step for that, unlike process_create.
     * It does not restart anything (USR-034, a separate, later item that
     * needs a way to map a fault's sender badge back to "which role
     * crashed" -- process_create doesn't return the id it allocated today).
     */
    inline constexpr word_t root_supervisor_role = 0x301U;
    inline constexpr capability_id_t supervisor_thread_selector = 150U;
    inline constexpr capability_id_t supervisor_space_selector = 151U;

    // Defined below, next to the fault-endpoint constants it depends on.
    inline void drain_fault_reports() noexcept;

    [[noreturn]] inline void supervision_thread_entry() noexcept {
        for (;;)
            drain_fault_reports();
    }

    [[nodiscard]] inline bool spawn_supervision_thread() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U;
        const auto found = platform::v1::earlyfs::find_span(page, directory_bound, "bin/init");
        if (!found.found)
            return false;
        if (control(abi::v1::control_operation::role_image_bind, root_supervisor_role,
                    static_cast<word_t>(found.offset), static_cast<word_t>(found.size)) != success)
            return false;
        return control(abi::v1::control_operation::thread_create, 0U, root_supervisor_role,
                       supervisor_thread_selector, supervisor_space_selector) == success;
    }

    [[nodiscard]] inline bool launch_path(const char* name, word_t cpu,
                                          capability_id_t thread_selector,
                                          capability_id_t task_selector,
                                          capability_id_t space_selector) noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U;
        const auto found = platform::v1::earlyfs::find_span(page, directory_bound, name);
        if (!found.found)
            return false;
        if (control(abi::v1::control_operation::role_image_bind, dynamic_launch_role,
                    static_cast<word_t>(found.offset), static_cast<word_t>(found.size)) != success)
            return false;
        return control(abi::v1::control_operation::process_create, cpu, dynamic_launch_role,
                       thread_selector, task_selector, space_selector) == success;
    }

    /*
     * Baseline crash handling: drain root's own fault endpoint (see
     * root_fault_endpoint above) and reply terminate immediately for
     * whatever is waiting there, instead of leaving a crashed child to sit
     * blocked_fault for the kernel's own 500-tick fault_timeout_ticks
     * before it silently times out to state::terminated. This only makes
     * termination prompt -- it does not report the crash anywhere
     * (deferred: production userspace has no logging syscall today, and
     * OBS-010 forbids raw fault address/PC/syndrome in release logs even
     * if one existed, so a redaction-compliant visibility mechanism needs
     * its own separate design pass) and does not restart anything
     * (USR-034, a separate, later item).
     *
     * ipc_receive() is a genuinely blocking primitive (unlike
     * notification_poll's instant memory read); a 1-tick bounded wait is
     * used to keep this from stalling the readiness-detection loop below
     * by more than one scheduler tick per iteration while a check is made.
     */
    inline constexpr word_t fault_poll_ticks = 1U;

    inline void drain_fault_reports() noexcept {
        const auto reply =
            ipc_receive(root_fault_endpoint, abi::v1::encode_timeout(fault_poll_ticks));
        if (reply.status != static_cast<word_t>(error_t::success))
            return;
        (void)ipc_reply(static_cast<word_t>(abi::v1::fault_disposition::terminate), 0U, 0U, 0U);
    }

    [[nodiscard]] inline int supervise() noexcept {
        if (!bind_role_images())
            return 1;
        if (control(abi::v1::control_operation::process_create, 1U, memory_role, memory_selector,
                    memory_selector + 1U,
                    memory_selector + 2U) != static_cast<word_t>(error_t::success))
            return 1;
        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index) {
            if (!launch(index))
                return 1;
            /*
             * Minted immediately after this specific launch(), not after
             * the whole loop: launch() schedules the new thread onto
             * index % 4U's CPU right away (process_create's IPI wakes
             * other CPUs), so the console-server could start running --
             * and call map_frame on the slot this mints into -- before
             * root reaches a later loop iteration or falls through the
             * loop entirely. Racing root's own mint against the
             * console-server's own map_frame is exactly the kind of
             * ordering bug this session already found the hard way once
             * (root_graph.hh's earlier earlyfs-binding fixes); minting
             * right here keeps it impossible by construction instead.
             */
            if (index == console_index && !mint_console_uart_frame())
                return 1;
        }

        const word_t expected =
            ((1U << abi::v1::control_plane_role_count) - 1U) | abi::v1::memory_service_ready_badge;
        word_t ready = 0U;
        bool console_verified = false;
        bool supervisor_spawned = false;
        for (;;) {
            drain_fault_reports();
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
                /*
                 * Proves the console-server can genuinely drive the UART
                 * from userspace: this text reaches the real serial output
                 * without any kernel printk call in its path. Guarded so it
                 * fires exactly once -- without CONFIG_GUEST_EMBEDDED_IMAGE
                 * this whole branch re-runs every loop iteration once ready.
                 */
                if (!console_verified) {
                    if (console::write(endpoint_base + console_index, "console-server alive\n") !=
                        static_cast<word_t>(error_t::success))
                        return 6;
                    console_verified = true;
                }
                if (!supervisor_spawned) {
                    if (!spawn_supervision_thread())
                        return 7;
                    supervisor_spawned = true;
                }
#if CONFIG_GUEST_EMBEDDED_IMAGE
                return start_embedded_guest() ? 0 : 5;
#endif
            }
        }
    }
} // namespace sys::root_graph
