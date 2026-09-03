#include <sys/console_client.hh>
#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/domain_manager.hh>
#include <sys/guest_manifest.hh>
#include <sys/hypervisor.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/platform/v1/earlyfs.hh>
#include <sys/thread.hh>
#include <sys/types.hh>
#include <sys/vmm/elf.hh>
#include <sys/vmm/vpl011.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace
{
    // Single source of truth in the native personality; see
    // src/user/personalities/native/README.md.
    inline constexpr sys::capability_id_t root_notification = sys::native::root_notification;
    inline constexpr sys::capability_id_t service_endpoint = sys::native::service_endpoint;
    inline constexpr sys::capability_id_t vm_selector = 61U;
    inline constexpr sys::capability_id_t vcpu_selector = 62U;
    inline constexpr sys::capability_id_t self_task_selector = 0U;
    inline constexpr sys::capability_id_t self_space_selector = sys::native::own_space;
    /*
     * Slots for device frames/IRQs minted by root (see
     * root_graph.hh::start_embedded_guest(), which uses the same base
     * values), one pair per sys::guest_manifest device entry.
     */
    [[maybe_unused]] inline constexpr sys::capability_id_t device_frame_base = 100U;
    [[maybe_unused]] inline constexpr sys::capability_id_t device_irq_base = 116U;
    [[maybe_unused]] inline constexpr sys::capability_id_t device_irq_notification_selector = 18U;
    // Minted by root_graph.hh::mint_embedded_guest_resources() -- console-
    // server's write/health/describe/stop endpoint, reused directly rather
    // than a new one.
    inline constexpr sys::capability_id_t console_endpoint_selector = 19U;
    // console-server now serves read_byte on a SEPARATE endpoint from a
    // second thread (see src/user/servers/console/main.cc's stdin_main());
    // this is that endpoint, minted alongside console_endpoint_selector.
    inline constexpr sys::capability_id_t console_stdin_endpoint_selector = 52U;
    inline constexpr sys::capability_id_t guest_frame_base = 20U;
    inline constexpr sys::word_t scratch_address = 0x2003f000U;
    inline constexpr sys::word_t guest_page_size = 4096U;
    inline constexpr sys::word_t guest_page_limit = 32U;
    inline constexpr sys::word_t failure_badge = sys::native::failure_badge;

    struct guest_page final {
        sys::capability_id_t slot{};
        sys::word_t ipa{};
    };

    // Moved to src/user/domains/vmm (sys/vmm/elf.hh): the on-disk guest
    // image format is VMM vocabulary, not this server's.
    using elf64_header = sys::vmm::elf::header;
    using elf64_program_header = sys::vmm::elf::program_header;
    using elf64_section_header = sys::vmm::elf::section_header;

    inline guest_page loaded_guest_pages[guest_page_limit]{};
    inline sys::word_t loaded_guest_page_count{};
    inline sys::word_t load_failure{};
    inline sys::word_t mapped_device_ipa[sys::guest_manifest::maximum_devices]{};
    inline bool mapped_device[sys::guest_manifest::maximum_devices]{};
    inline sys::domain_manager::manager domain{};

    /*
     * vPL011: the guest's UART (manifest has zero passthrough devices, see
     * samples/guests/zephyr/manifest.cc) is trapped and emulated here
     * instead, with real character I/O forwarded through the
     * console-server. Only the register state this guest's actual PL011
     * driver touches is modeled (see the plan doc's investigation of the
     * fetched Zephyr driver source): DR, FR (RXFE/TXFF), IMSC (RXIM), and
     * CR are real; everything else in the 4 KiB IPA window is a safe
     * write-and-discard / read-returns-0.
     */
    // Moved to src/user/domains/vmm (sys/vmm/vpl011.hh): the device model
    // is VMM vocabulary. The capability-wielding glue below -- console
    // forwarding, IRQ injection, guest PC advance -- stays here, because it
    // needs capabilities this server holds and the model does not.
    namespace vpl011 = sys::vmm::vpl011;

    /*
     * Flushes any buffered vPL011 TX bytes through the console-server's
     * existing NUL-terminated string write() op. A literal NUL byte
     * mid-buffer would truncate the rest of that flush -- guest console
     * text is never binary, so this matches write()'s own already-
     * documented truncation behavior rather than adding a new
     * length-based op just for this. No-op (no IPC call) when nothing is
     * buffered, so calling it liberally on idle exits is cheap.
     */
    inline void flush_console_output() noexcept {
        if (vpl011::tx_length == 0U)
            return;
        vpl011::tx_buffer[vpl011::tx_length] = 0U;
        (void)sys::console::write(console_endpoint_selector,
                                  reinterpret_cast<const char*>(vpl011::tx_buffer));
        vpl011::tx_length = 0U;
    }

    [[nodiscard]] inline constexpr sys::word_t load_error(sys::word_t stage,
                                                          sys::word_t status) noexcept {
        return (stage << 32U) | (status & 0xffffffffU);
    }

#if CONFIG_GUEST_EMBEDDED_IMAGE
    extern "C" const sys::u8 sys_arm64_guest_earlyfs_image_start[];
    extern "C" const sys::u8 sys_arm64_guest_earlyfs_image_end[];

    [[nodiscard]] inline sys::platform::v1::earlyfs::view guest_elf_view() noexcept {
        const auto size = static_cast<sys::usize_t>(sys_arm64_guest_earlyfs_image_end -
                                                    sys_arm64_guest_earlyfs_image_start);
        return sys::platform::v1::earlyfs::find(sys_arm64_guest_earlyfs_image_start, size,
                                                "guest.elf");
    }
#endif

    [[maybe_unused]] inline void cleanup_loaded_guest() noexcept {
        for (sys::word_t index = 0U; index < sys::guest_manifest::maximum_devices; ++index) {
            if (mapped_device[index]) {
                (void)domain.vm.unmap(mapped_device_ipa[index]);
                mapped_device[index] = false;
            }
        }
        for (sys::word_t index = loaded_guest_page_count; index != 0U; --index) {
            const guest_page page = loaded_guest_pages[index - 1U];
            (void)domain.vm.unmap(page.ipa);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, page.slot);
            loaded_guest_pages[index - 1U] = {};
        }
        loaded_guest_page_count = 0U;
    }

    [[nodiscard]] inline sys::word_t guest_permissions(sys::u32 flags) noexcept {
        sys::word_t permissions = static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read);
        if ((flags & 1U) != 0U)
            permissions |= static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::execute);
        if ((flags & 2U) != 0U)
            permissions |= static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        return permissions;
    }

    [[nodiscard]] inline bool stage_guest_page(sys::word_t page_index, sys::word_t ipa,
                                               const sys::u8* src, sys::word_t length,
                                               sys::u32 flags) noexcept {
        if (page_index >= guest_page_limit) {
            load_failure = sys::word_t{1U} << 32U;
            return false;
        }

        const sys::capability_id_t slot = guest_frame_base + page_index;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t read_write =
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::normal, sys::abi::v1::memory_shareability::inner_shareable);

        const sys::word_t created =
            sys::control(sys::abi::v1::control_operation::frame_create, self_task_selector, slot);
        if (created != success) {
            load_failure = load_error(2U, created);
            return false;
        }
        const sys::word_t mapped =
            sys::control(sys::abi::v1::control_operation::map_frame, self_space_selector, slot,
                         scratch_address, read_write, attrs);
        if (mapped != success) {
            load_failure = load_error(3U, mapped);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, slot);
            return false;
        }

        auto* const destination = reinterpret_cast<volatile sys::u8*>(scratch_address);
        for (sys::word_t offset = 0U; offset < guest_page_size; ++offset) {
            const sys::word_t value = offset < length ? src[offset] : 0U;
            destination[offset] = static_cast<sys::u8>(value);
        }

        const sys::word_t unmapped = sys::control(sys::abi::v1::control_operation::unmap_frame,
                                                  self_space_selector, slot, scratch_address);
        if (unmapped != success) {
            load_failure = load_error(4U, unmapped);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, slot);
            return false;
        }

        const sys::word_t stage2 = domain.vm.map_frame(ipa, slot, guest_permissions(flags));
        if (stage2 != success) {
            load_failure = load_error(5U, stage2);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, slot);
            return false;
        }

        loaded_guest_pages[page_index] = {.slot = slot, .ipa = ipa};
        if (loaded_guest_page_count < page_index + 1U)
            loaded_guest_page_count = page_index + 1U;
        return true;
    }

    /*
     * Forward whichever physical device interrupts (per the guest manifest)
     * are owned in userspace into the guest's vGIC. This replaces what used
     * to be a hardcoded EL2 hack ("if physical irq == 33, poke the guest's
     * GIC state directly"): the kernel/hypervisor have no idea any of these
     * devices or IRQs exist. Ownership comes from capabilities minted by
     * root (root_graph.hh) and bound to a single shared notification;
     * forwarding is a non-blocking poll folded into the existing
     * wait/virtual_timer re-entry points below, since this codebase has no
     * blocking-wait syscall for notifications.
     */
    inline void forward_device_irqs() noexcept {
        const auto& manifest = ::sys_arm64_domain_guest_manifest;
        sys::word_t signaled = 0U;
        const sys::word_t polled =
            sys::control_result1(signaled, sys::abi::v1::control_operation::notification_poll,
                                 device_irq_notification_selector);
        if (polled != static_cast<sys::word_t>(sys::error_t::success) || signaled == 0U)
            return;
        for (sys::word_t index = 0U; index < manifest.device_count; ++index) {
            const auto& dev = manifest.devices[index];
            if (dev.forward_irq == sys::guest_manifest::no_irq)
                continue;
            if ((signaled & (sys::word_t{1U} << (dev.forward_irq & 63U))) == 0U)
                continue;
            (void)domain.vm.inject(static_cast<sys::u16>(dev.forward_irq));
            (void)sys::control(sys::abi::v1::control_operation::interrupt_ack,
                               device_irq_base + index);
        }
    }

    /*
     * Polls the console-server for one real-hardware RX byte and, if the
     * guest has RX interrupts unmasked (IMSC.RXIM), injects vPL011's
     * virtual IRQ -- same domain.vm.inject() call forward_device_irqs()
     * already uses for the passthrough case, just software-triggered
     * instead of driven by a real interrupt line. Called once per serve()
     * iteration, same placement convention as forward_device_irqs().
     */
    inline void forward_console_input() noexcept {
        if (vpl011::rx_pending)
            return;
        const auto polled = sys::console::read_byte(console_stdin_endpoint_selector);
        if (!polled.available)
            return;
        vpl011::rx_pending = true;
        vpl011::rx_byte = polled.value;
        if ((vpl011::imsc & vpl011::imsc_rxim) != 0U)
            (void)domain.vm.inject(vpl011::irq);
    }

    /*
     * Resolves an mmio VM exit against the vPL011 IPA window. Returns
     * false (untouched) for anything outside that window, so the caller's
     * existing terminal-exit handling still applies unchanged for real
     * unexpected accesses. Follows the same decode-emulate-advance-PC
     * pattern as the kernel's own emulate_native_gic_mmio() (src/arch/
     * arm64/include/sys/arch/hypervisor.hh), just from userspace via
     * vcpu_state_read/write instead of direct register-frame access.
     */
    [[nodiscard]] inline bool
    handle_vpl011_mmio(const sys::abi::v1::vm_exit_result& exit) noexcept {
        /*
         * exit.fault_address is populated from raw FAR_EL2, which for a
         * stage-2-only abort (guest stage-1 already translated the access)
         * is not a reliable IPA -- it can be a guest virtual address if the
         * guest's own MMU maps the device at a different VA than its IPA,
         * which this Zephyr build does. The properly reconstructed IPA
         * (HPFAR_EL2 combined with the page offset) is what the kernel
         * packs into qualification's low 48 bits instead -- see
         * arch::hypervisor::guest_fault_ipa()/mmio_qualification() in
         * src/arch/arm64/include/sys/arch/hypervisor.hh. That's the field
         * to use here, not fault_address.
         */
        const sys::word_t ipa = exit.qualification & 0x0000ffffffffffffULL;
        if (ipa < vpl011::base_ipa || ipa >= vpl011::base_ipa + vpl011::size)
            return false;
        const sys::word_t offset = ipa - vpl011::base_ipa;
        const bool write = ((exit.qualification >> vpl011::qualification_write_bit) & 1U) != 0U;
        const sys::word_t target = (exit.qualification >> vpl011::qualification_srt_shift) &
                                   vpl011::qualification_srt_mask;

        sys::word_t value = 0U;
        if (write && target != vpl011::xzr_register) {
            const auto read = sys::vcpu_state_read(domain.vm.vcpu, target);
            value = read.value;
        }

        switch (offset) {
            case vpl011::offset_dr:
                if (write) {
                    vpl011::tx_buffer[vpl011::tx_length++] = static_cast<sys::u8>(value);
                    if (vpl011::tx_length == vpl011::tx_buffer_capacity)
                        flush_console_output();
                } else {
                    value = vpl011::rx_pending ? static_cast<sys::word_t>(vpl011::rx_byte) : 0U;
                    vpl011::rx_pending = false;
                }
                break;
            case vpl011::offset_fr:
                if (!write)
                    value = vpl011::rx_pending ? 0U : vpl011::fr_rxfe;
                break;
            case vpl011::offset_imsc:
                if (write)
                    vpl011::imsc = value;
                else
                    value = vpl011::imsc;
                break;
            case vpl011::offset_cr:
                if (write)
                    vpl011::cr = value;
                else
                    value = vpl011::cr;
                break;
            default:
                // LCR_H, IBRD, FBRD, IFLS, ICR, DMACR, RIS, MIS, etc.: this
                // guest's actual driver never reads these back in a way
                // that affects behavior (confirmed against its fetched
                // source) -- accept writes silently, read back 0.
                break;
        }

        if (!write && target != vpl011::xzr_register)
            (void)sys::vcpu_state_write(domain.vm.vcpu, target, value);
        (void)sys::vcpu_state_write(domain.vm.vcpu, vpl011::pc_field, exit.guest_pc + 4U);
        return true;
    }

    /*
     * Map every passthrough device the manifest declares and bind a
     * notification to any of their forwarded physical IRQs. Root has
     * already created and minted the underlying frame/interrupt
     * capabilities at device_frame_base/device_irq_base + index (see
     * root_graph.hh::start_embedded_guest()); this just consumes them.
     */
    [[maybe_unused]] [[nodiscard]] inline sys::word_t
    map_manifest_devices(const sys::guest_manifest::manifest& manifest) noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        bool any_irq = false;
        for (sys::word_t index = 0U; index < manifest.device_count; ++index) {
            const auto& dev = manifest.devices[index];
            const sys::word_t mapped =
                domain.vm.map_frame(dev.ipa, device_frame_base + index, dev.permissions);
            if (mapped != success)
                return mapped;
            mapped_device_ipa[index] = dev.ipa;
            mapped_device[index] = true;
            if (dev.forward_irq != sys::guest_manifest::no_irq)
                any_irq = true;
        }
        if (!any_irq)
            return success;
        const sys::word_t created = sys::control(
            sys::abi::v1::control_operation::notification_create, device_irq_notification_selector);
        if (created != success)
            return created;
        for (sys::word_t index = 0U; index < manifest.device_count; ++index) {
            if (manifest.devices[index].forward_irq == sys::guest_manifest::no_irq)
                continue;
            const sys::word_t bound =
                sys::control(sys::abi::v1::control_operation::interrupt_bind,
                             device_irq_base + index, device_irq_notification_selector);
            if (bound != success)
                return bound;
        }
        return success;
    }

    [[nodiscard]] inline bool guest_page_loaded(sys::word_t ipa) noexcept {
        for (sys::word_t index = 0U; index < loaded_guest_page_count; ++index)
            if (loaded_guest_pages[index].ipa == ipa)
                return true;
        return false;
    }

    [[nodiscard]] inline sys::u32 elf_page_flags(const elf64_header& header, const sys::u8* begin,
                                                 sys::word_t size, sys::word_t ipa) noexcept {
        constexpr sys::u64 section_alloc = 1U << 1U;
        constexpr sys::u64 section_write = 1U << 0U;
        constexpr sys::u64 section_execute = 1U << 2U;
        if (header.shentsize != sizeof(elf64_section_header) || header.shoff > size ||
            header.shnum > (size - header.shoff) / header.shentsize)
            return 0U;

        const auto* sections = reinterpret_cast<const elf64_section_header*>(begin + header.shoff);
        sys::u32 flags = 4U;
        for (sys::word_t index = 0U; index < header.shnum; ++index) {
            const elf64_section_header& section = sections[index];
            if ((section.flags & section_alloc) == 0U || section.size == 0U)
                continue;
            if (section.address + section.size < section.address ||
                ipa >= section.address + section.size || section.address >= ipa + guest_page_size)
                continue;
            if ((section.flags & section_write) != 0U)
                flags |= 2U;
            if ((section.flags & section_execute) != 0U)
                flags |= 1U;
        }
        return (flags & 3U) == 3U ? 0U : flags;
    }

    [[maybe_unused]] [[nodiscard]] inline bool load_guest_image(const sys::u8* begin,
                                                                const sys::u8* end,
                                                                sys::word_t ram_size,
                                                                sys::word_t& entry) noexcept {
        cleanup_loaded_guest();
        load_failure = 0U;
        if (begin == nullptr || end == nullptr || end <= begin) {
            load_failure = sys::word_t{6U} << 32U;
            return false;
        }

        const sys::word_t size = static_cast<sys::word_t>(end - begin);
        const auto* header = reinterpret_cast<const elf64_header*>(begin);
        const bool elf =
            size >= sizeof(elf64_header) && header->ident[0] == 0x7fU && header->ident[1] == 'E' &&
            header->ident[2] == 'L' && header->ident[3] == 'F' && header->ident[4] == 2U &&
            header->ident[5] == 1U && header->machine == 0xb7U && header->version == 1U;

        if (!elf) {
            entry = 0U;
            sys::word_t page_index = 0U;
            for (sys::word_t offset = 0U; offset < size; offset += guest_page_size) {
                const sys::word_t chunk =
                    size - offset < guest_page_size ? size - offset : guest_page_size;
                if (!stage_guest_page(page_index, offset, begin + offset, chunk, 1U | 4U)) {
                    cleanup_loaded_guest();
                    return false;
                }
                ++page_index;
            }
            return true;
        }

        entry = static_cast<sys::word_t>(header->entry);
        if (header->phentsize != sizeof(elf64_program_header) || header->phoff > size ||
            header->phnum > (size - header->phoff) / header->phentsize)
            return false;
        const auto* program_headers = reinterpret_cast<const elf64_program_header*>(
            begin + static_cast<sys::word_t>(header->phoff));
        sys::word_t page_index = 0U;
        sys::word_t ram_base = ~sys::word_t{0U};
        for (sys::word_t ph = 0U; ph < header->phnum; ++ph) {
            const elf64_program_header& program = program_headers[ph];
            if (program.type != 1U)
                continue;
            if (program.filesz > program.memsz || program.offset + program.filesz > size) {
                cleanup_loaded_guest();
                return false;
            }

            const sys::word_t base =
                static_cast<sys::word_t>(program.paddr != 0U ? program.paddr : program.vaddr);
            if ((base & (guest_page_size - 1U)) != 0U) {
                cleanup_loaded_guest();
                return false;
            }
            if (base < ram_base)
                ram_base = base;
            for (sys::word_t offset = 0U; offset < program.memsz; offset += guest_page_size) {
                if (page_index >= guest_page_limit) {
                    cleanup_loaded_guest();
                    return false;
                }
                const sys::word_t file_remaining =
                    program.filesz > offset ? program.filesz - offset : 0U;
                const sys::word_t chunk =
                    file_remaining < guest_page_size ? file_remaining : guest_page_size;
                const sys::word_t ipa = base + offset;
                const sys::u32 flags = elf_page_flags(*header, begin, size, ipa);
                const sys::u8* source = chunk != 0U ? begin + program.offset + offset : nullptr;
                if (flags == 0U || !stage_guest_page(page_index, ipa, source, chunk, flags)) {
                    if (flags == 0U)
                        load_failure = sys::word_t{7U} << 32U;
                    cleanup_loaded_guest();
                    return false;
                }
                ++page_index;
            }
        }

        if (ram_base == ~sys::word_t{0U}) {
            cleanup_loaded_guest();
            return false;
        }
        for (sys::word_t offset = 0U; offset < ram_size; offset += guest_page_size) {
            const sys::word_t ipa = ram_base + offset;
            if (guest_page_loaded(ipa))
                continue;
            const sys::u32 flags = elf_page_flags(*header, begin, size, ipa);
            if (flags == 0U || !stage_guest_page(page_index, ipa, nullptr, 0U, flags)) {
                if (flags == 0U)
                    load_failure = sys::word_t{8U} << 32U;
                cleanup_loaded_guest();
                return false;
            }
            ++page_index;
        }
        return true;
    }
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    const auto policy = sys::control_plane::policy_for(role);
    const sys::word_t ready = sys::abi::v1::control_plane_ready_badge(role);
    if (!sys::control_plane::valid(policy) || ready == 0U) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, root_notification,
                           failure_badge);
        return 1;
    }

    const sys::word_t status = sys::control(sys::abi::v1::control_operation::notification_signal,
                                            root_notification, ready);
    if (status != static_cast<sys::word_t>(sys::error_t::success))
        return 2;

    for (;;) {
        const auto request = sys::ipc_receive(service_endpoint);
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        const auto operation = static_cast<sys::abi::v1::control_plane_operation>(request.message0);
        sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        sys::word_t result1 = 0U;
        if (operation == sys::abi::v1::control_plane_operation::health) {
            result0 = sys::abi::v1::control_plane_health_magic;
            result1 = role;
        } else if (operation == sys::abi::v1::control_plane_operation::describe) {
            result0 = policy.dependency_mask;
            result1 = (policy.quota_pages << 16U) | policy.restart_limit;
        } else if (operation == sys::abi::v1::control_plane_operation::launch) {
            result0 = domain.launch(vm_selector, vcpu_selector, 0U, 0x3000U);
            if (result0 == static_cast<sys::word_t>(sys::error_t::success)) {
                result1 = static_cast<sys::word_t>(domain.lifecycle);
            }
        } else if (operation == sys::abi::v1::control_plane_operation::load) {
#if CONFIG_GUEST_EMBEDDED_IMAGE
            const auto& manifest = ::sys_arm64_domain_guest_manifest;
            const auto guest_elf = guest_elf_view();
            sys::word_t entry = 0U;
            result0 = sys::guest_manifest::valid(manifest) && guest_elf.valid() &&
                              load_guest_image(guest_elf.data, guest_elf.data + guest_elf.size,
                                               manifest.ram_size, entry)
                          ? map_manifest_devices(manifest)
                          : static_cast<sys::word_t>(sys::error_t::invalid_argument);
            if (result0 == static_cast<sys::word_t>(sys::error_t::success))
                result0 = domain.configure(entry, manifest.guest_pstate, manifest.guest_stack);
            if (result0 == static_cast<sys::word_t>(sys::error_t::success))
                result1 = static_cast<sys::word_t>(domain.lifecycle);
            else
                result1 = load_failure != 0U ? load_failure : result0;
#else
            result0 = static_cast<sys::word_t>(sys::error_t::unsupported);
#endif
        } else if (operation == sys::abi::v1::control_plane_operation::run) {
            if (domain.lifecycle != sys::domain_manager::state::runnable) {
                result0 = static_cast<sys::word_t>(sys::error_t::busy);
            } else {
                /* Before vPL011, the guest's UART was direct passthrough,
                 * so it never trapped and one domain.run() call already
                 * carried the guest through its entire boot sequence to
                 * its first genuine wait/virtual_timer exit. Now UART
                 * accesses are real vm-exits that a single run() call
                 * would otherwise stop at immediately (the first register
                 * touch, well before the boot banner). Looping over
                 * handled mmio exits here restores that original
                 * single-call semantic instead of changing it. */
                for (;;) {
                    const auto& exit = domain.run();
                    if (exit.status == static_cast<sys::word_t>(sys::error_t::success) &&
                        exit.reason == sys::abi::v1::vm_exit_reason::mmio &&
                        handle_vpl011_mmio(exit))
                        continue;
                    flush_console_output();
                    result0 = exit.status;
                    result1 = static_cast<sys::word_t>(exit.reason);
                    break;
                }
            }
        } else if (operation == sys::abi::v1::control_plane_operation::serve) {
            if (domain.lifecycle != sys::domain_manager::state::runnable) {
                result0 = static_cast<sys::word_t>(sys::error_t::busy);
            } else {
                for (;;) {
                    forward_device_irqs();
                    const auto& exit = domain.run();
                    if (exit.status == static_cast<sys::word_t>(sys::error_t::success) &&
                        (exit.reason == sys::abi::v1::vm_exit_reason::wait ||
                         exit.reason == sys::abi::v1::vm_exit_reason::virtual_timer)) {
                        // forward_console_input() is a blocking IPC call to
                        // the console-server, only cheap when nothing is
                        // pending; measured at ~2.7x the dominant cost of
                        // TX throughput when called on every loop iteration
                        // (as it originally was), since the guest's TX poll
                        // pattern (read FR, write DR) produces two mmio
                        // exits per output byte and each one paid this
                        // call's scheduling cost. Only polling here, on
                        // genuinely idle exits, keeps RX responsive between
                        // characters without taxing every register access
                        // during an active print burst -- the console-
                        // server and real hardware FIFO both cushion a few
                        // bytes typed while the guest is mid-output, so
                        // this doesn't drop input, just delays noticing it
                        // until the guest is next idle.
                        forward_console_input();
                        // Flush promptly on idle rather than waiting for the
                        // buffer to fill -- a partial line (e.g. a shell
                        // prompt) should still appear without delay. No-op
                        // when nothing is buffered, so this costs nothing
                        // on the common (no pending output) tick.
                        flush_console_output();
                        continue;
                    }
                    if (exit.status == static_cast<sys::word_t>(sys::error_t::success) &&
                        exit.reason == sys::abi::v1::vm_exit_reason::mmio &&
                        handle_vpl011_mmio(exit))
                        continue;
                    flush_console_output();
                    result0 = exit.status;
                    result1 = (static_cast<sys::word_t>(exit.reason) << 32U) |
                              (exit.qualification & 0xffffffffU);
                    break;
                }
            }
        } else if (operation == sys::abi::v1::control_plane_operation::destroy) {
            cleanup_loaded_guest();
            result0 = domain.destroy();
            if (result0 == static_cast<sys::word_t>(sys::error_t::success))
                result1 = static_cast<sys::word_t>(domain.lifecycle);
        } else if (operation == sys::abi::v1::control_plane_operation::stop) {
            cleanup_loaded_guest();
            if (domain.lifecycle != sys::domain_manager::state::empty)
                (void)domain.destroy();
            const sys::word_t replied = sys::ipc_reply(0U, role, 0U, 0U);
            if (replied != static_cast<sys::word_t>(sys::error_t::success))
                return 3;
            sys::thread_exit(0U, root_notification, sys::abi::v1::control_plane_exit_badge(role));
        }
        if (sys::ipc_reply(result0, result1, 0U, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
