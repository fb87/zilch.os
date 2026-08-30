#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/domain_manager.hh>
#include <sys/ipc.hh>
#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/memory.hh>

namespace
{
    inline constexpr sys::capability_id_t root_notification = 14U;
    inline constexpr sys::capability_id_t service_endpoint = 11U;
    inline constexpr sys::capability_id_t vm_selector = 61U;
    inline constexpr sys::capability_id_t vcpu_selector = 62U;
    inline constexpr sys::capability_id_t self_task_selector = 0U;
    inline constexpr sys::capability_id_t self_space_selector = 3U;
    [[maybe_unused]] inline constexpr sys::capability_id_t pl011_frame_selector = 16U;
    inline constexpr sys::capability_id_t guest_frame_base = 20U;
    inline constexpr sys::word_t scratch_address = 0x2003f000U;
    inline constexpr sys::word_t guest_page_size = 4096U;
    inline constexpr sys::word_t guest_page_limit = 32U;
    inline constexpr sys::word_t guest_ram_size = 32U * guest_page_size;
    [[maybe_unused]] inline constexpr sys::word_t guest_stack = 0xf000U;
    [[maybe_unused]] inline constexpr sys::word_t guest_pstate = 0x3c5U;
    inline constexpr sys::word_t pl011_ipa = 0x09000000U;
    [[maybe_unused]] inline constexpr sys::word_t pl011_permissions = 1U | 2U | (1U << 3U);
    inline constexpr sys::word_t failure_badge = 1U << 15U;

    struct guest_page final {
        sys::capability_id_t slot{};
        sys::word_t ipa{};
    };

    struct elf64_header final {
        sys::u8 ident[16U];
        sys::u16 type{};
        sys::u16 machine{};
        sys::u32 version{};
        sys::u64 entry{};
        sys::u64 phoff{};
        sys::u64 shoff{};
        sys::u32 flags{};
        sys::u16 ehsize{};
        sys::u16 phentsize{};
        sys::u16 phnum{};
        sys::u16 shentsize{};
        sys::u16 shnum{};
        sys::u16 shstrndx{};
    } __attribute__((packed));

    struct elf64_program_header final {
        sys::u32 type{};
        sys::u32 flags{};
        sys::u64 offset{};
        sys::u64 vaddr{};
        sys::u64 paddr{};
        sys::u64 filesz{};
        sys::u64 memsz{};
        sys::u64 align{};
    } __attribute__((packed));

    struct elf64_section_header final {
        sys::u32 name{};
        sys::u32 type{};
        sys::u64 flags{};
        sys::u64 address{};
        sys::u64 offset{};
        sys::u64 size{};
        sys::u32 link{};
        sys::u32 info{};
        sys::u64 addralign{};
        sys::u64 entsize{};
    } __attribute__((packed));

    inline guest_page loaded_guest_pages[guest_page_limit]{};
    inline sys::word_t loaded_guest_page_count{};
    inline sys::word_t load_failure{};
    inline bool pl011_mapped{};
    inline sys::domain_manager::manager domain{};

    [[nodiscard]] inline constexpr sys::word_t load_error(sys::word_t stage,
                                                           sys::word_t status) noexcept {
        return (stage << 32U) | (status & 0xffffffffU);
    }

#if CONFIG_GUEST_EMBEDDED_IMAGE
    extern "C" const sys::u8 sys_arm64_domain_guest_image_start[];
    extern "C" const sys::u8 sys_arm64_domain_guest_image_end[];
#endif

    [[maybe_unused]] inline void cleanup_loaded_guest() noexcept {
        if (pl011_mapped) {
            (void)domain.vm.unmap(pl011_ipa);
            pl011_mapped = false;
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
        const sys::word_t read_write = static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
                                       static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::normal, sys::abi::v1::memory_shareability::inner_shareable);

        const sys::word_t created = sys::control(sys::abi::v1::control_operation::frame_create,
                                                  self_task_selector, slot);
        if (created != success) {
            load_failure = load_error(2U, created);
            return false;
        }
        const sys::word_t mapped = sys::control(sys::abi::v1::control_operation::map_frame,
                                                 self_space_selector, slot, scratch_address,
                                                 read_write, attrs);
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

    [[nodiscard]] inline bool guest_page_loaded(sys::word_t ipa) noexcept {
        for (sys::word_t index = 0U; index < loaded_guest_page_count; ++index)
            if (loaded_guest_pages[index].ipa == ipa)
                return true;
        return false;
    }

    [[nodiscard]] inline sys::u32 elf_page_flags(const elf64_header& header,
                                                  const sys::u8* begin, sys::word_t size,
                                                  sys::word_t ipa) noexcept {
        constexpr sys::u64 section_alloc = 1U << 1U;
        constexpr sys::u64 section_write = 1U << 0U;
        constexpr sys::u64 section_execute = 1U << 2U;
        if (header.shentsize != sizeof(elf64_section_header) ||
            header.shoff > size || header.shnum > (size - header.shoff) / header.shentsize)
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
                                                                sys::word_t& entry) noexcept {
        cleanup_loaded_guest();
        load_failure = 0U;
        if (begin == nullptr || end == nullptr || end <= begin) {
            load_failure = sys::word_t{6U} << 32U;
            return false;
        }

        const sys::word_t size = static_cast<sys::word_t>(end - begin);
        const auto* header = reinterpret_cast<const elf64_header*>(begin);
        const bool elf = size >= sizeof(elf64_header) && header->ident[0] == 0x7fU &&
                         header->ident[1] == 'E' && header->ident[2] == 'L' &&
                         header->ident[3] == 'F' && header->ident[4] == 2U &&
                         header->ident[5] == 1U && header->machine == 0xb7U &&
                         header->version == 1U;

        if (!elf) {
            entry = 0U;
            sys::word_t page_index = 0U;
            for (sys::word_t offset = 0U; offset < size; offset += guest_page_size) {
                const sys::word_t chunk = size - offset < guest_page_size ? size - offset
                                                                          : guest_page_size;
                if (!stage_guest_page(page_index, offset, begin + offset, chunk,
                                      1U | 4U)) {
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

            const sys::word_t base = static_cast<sys::word_t>(program.paddr != 0U ? program.paddr
                                                                                   : program.vaddr);
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
                const sys::word_t file_remaining = program.filesz > offset ? program.filesz - offset
                                                                           : 0U;
                const sys::word_t chunk = file_remaining < guest_page_size ? file_remaining
                                                                            : guest_page_size;
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
        for (sys::word_t offset = 0U; offset < guest_ram_size; offset += guest_page_size) {
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
            sys::word_t entry = 0U;
            result0 = load_guest_image(sys_arm64_domain_guest_image_start,
                                       sys_arm64_domain_guest_image_end, entry)
                          ? domain.vm.map_frame(pl011_ipa, pl011_frame_selector, pl011_permissions)
                          : static_cast<sys::word_t>(sys::error_t::invalid_argument);
            pl011_mapped = result0 == static_cast<sys::word_t>(sys::error_t::success);
            if (pl011_mapped)
                result0 = domain.configure(entry, guest_pstate, guest_stack);
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
                const auto& exit = domain.run();
                result0 = exit.status;
                result1 = static_cast<sys::word_t>(exit.reason);
            }
        } else if (operation == sys::abi::v1::control_plane_operation::serve) {
            if (domain.lifecycle != sys::domain_manager::state::runnable) {
                result0 = static_cast<sys::word_t>(sys::error_t::busy);
            } else {
                for (;;) {
                    const auto& exit = domain.run();
                    if (exit.status != static_cast<sys::word_t>(sys::error_t::success) ||
                        (exit.reason != sys::abi::v1::vm_exit_reason::wait &&
                         exit.reason != sys::abi::v1::vm_exit_reason::virtual_timer)) {
                        result0 = exit.status;
                        result1 = (static_cast<sys::word_t>(exit.reason) << 32U) |
                                  (exit.qualification & 0xffffffffU);
                        break;
                    }
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
