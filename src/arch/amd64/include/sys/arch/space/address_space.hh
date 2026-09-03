#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/memory.hh>
#include <sys/arch/space/asid.hh>
#include <sys/arch/space/elf64.hh>
#include <sys/platform/v1/earlyfs.hh>
#include <sys/types.hh>

namespace sys::arch::space
{
    inline constexpr bool user_available = true;
#if CONFIG_ROOT_ONLY_BOOT
    inline constexpr vaddr_t user_code = 0x20000000ULL;
    inline constexpr vaddr_t user_stack_base = user_code + elf64::bootstrap_size;
#else
    inline constexpr vaddr_t user_code = 0x10000000ULL;
    inline constexpr vaddr_t user_stack_base = user_code + elf64::bootstrap_size;
#endif

    inline constexpr vaddr_t kernel_identity_base = 0x40000000ULL;
    static_assert(user_code < kernel_identity_base);
    static_assert(user_stack_base < kernel_identity_base);

#if CONFIG_ROOT_ONLY_BOOT
    extern "C" char sys_amd64_earlyfs_image_start[];
    extern "C" char sys_amd64_earlyfs_image_end[];
#else
    extern "C" char sys_amd64_user_image_start[];
    extern "C" char sys_amd64_user_image_end[];
#endif

    inline constexpr word_t memory_server_image_role = 0x100U;
    inline constexpr word_t pager_client_image_role_base = 0x101U;
    inline constexpr word_t memory_client_image_role_base = 0x103U;
    inline constexpr word_t undefined_instruction_image_role = 0x106U;
    inline constexpr word_t ipc_lifecycle_client_role_base = 0x110U;
    inline constexpr word_t control_plane_image_role_base = 0x200U;
    inline constexpr word_t control_plane_image_role_count = 5U;
    inline constexpr word_t domain_manager_image_role = 0x203U;
    inline constexpr word_t console_server_image_role = 0x202U;
    inline constexpr word_t console_stdin_image_role = 0x108U;

    struct image_view {
        char* start;
        char* end;
    };

    inline constexpr word_t maximum_role_bindings = 16U;
    struct role_binding {
        word_t role{};
        u64 offset{};
        u64 size{};
        bool used{};
    };
    inline role_binding role_bindings[maximum_role_bindings]{};

#if CONFIG_ROOT_ONLY_BOOT
    [[nodiscard]] inline usize_t earlyfs_image_size() noexcept {
        return static_cast<usize_t>(sys_amd64_earlyfs_image_end - sys_amd64_earlyfs_image_start);
    }

    [[nodiscard]] inline bool earlyfs_page_address(word_t page_index, paddr_t& address) noexcept {
        const usize_t page_count =
            (earlyfs_image_size() + memory::page_size - 1U) / memory::page_size;
        if (page_index >= page_count)
            return false;
        address = reinterpret_cast<paddr_t>(sys_amd64_earlyfs_image_start) +
                  static_cast<paddr_t>(page_index) * memory::page_size;
        return true;
    }
#else
    [[nodiscard]] inline usize_t earlyfs_image_size() noexcept {
        return 0U;
    }

    [[nodiscard]] inline bool earlyfs_page_address(word_t, paddr_t&) noexcept {
        return false;
    }
#endif

    [[nodiscard]] inline error_t bind_role_image(word_t role, u64 offset, u64 size) noexcept {
        const auto image_size = static_cast<u64>(earlyfs_image_size());
        if (size == 0U || offset > image_size || size > image_size - offset)
            return error_t::invalid_argument;
        role_binding* slot = nullptr;
        for (auto& entry : role_bindings) {
            if (entry.used && entry.role == role) {
                slot = &entry;
                break;
            }
            if (slot == nullptr && !entry.used)
                slot = &entry;
        }
        if (slot == nullptr)
            return error_t::no_memory;
        slot->role = role;
        slot->offset = offset;
        slot->size = size;
        slot->used = true;
        return error_t::success;
    }

    [[nodiscard]] inline bool find_role_binding(word_t role, u64& offset, u64& size) noexcept {
        for (const auto& entry : role_bindings) {
            if (entry.used && entry.role == role) {
                offset = entry.offset;
                size = entry.size;
                return true;
            }
        }
        return false;
    }

#if CONFIG_ROOT_ONLY_BOOT
    [[nodiscard]] inline image_view image_for_role(word_t role) noexcept {
        u64 bound_offset = 0U;
        u64 bound_size = 0U;
        if (find_role_binding(role, bound_offset, bound_size)) {
            auto* start = sys_amd64_earlyfs_image_start + bound_offset;
            return {start, start + bound_size};
        }

        const char* name = "bin/init";
        if (role == memory_server_image_role)
            name = "bin/memory-server";
        else if (role == console_stdin_image_role)
            name = "bin/console-server";
#if CONFIG_TESTS
        else if (role == pager_client_image_role_base ||
                 role == pager_client_image_role_base + 1U ||
                 role == undefined_instruction_image_role ||
                 role == ipc_lifecycle_client_role_base ||
                 role == ipc_lifecycle_client_role_base + 1U ||
                 role == ipc_lifecycle_client_role_base + 2U ||
                 role == ipc_lifecycle_client_role_base + 3U ||
                 role == ipc_lifecycle_client_role_base + 4U ||
                 role == ipc_lifecycle_client_role_base + 5U ||
                 role == ipc_lifecycle_client_role_base + 6U ||
                 role == ipc_lifecycle_client_role_base + 7U)
            name = "bin/pager-client";
        else if (role >= memory_client_image_role_base && role < memory_client_image_role_base + 3U)
            name = "bin/memory-client";
#endif
        else if (role >= control_plane_image_role_base &&
                 role < control_plane_image_role_base + control_plane_image_role_count)
            name = role == domain_manager_image_role   ? "bin/domain-manager"
                   : role == console_server_image_role ? "bin/console-server"
                                                       : "bin/control-plane";

        const auto* earlyfs_begin = reinterpret_cast<const u8*>(sys_amd64_earlyfs_image_start);
        const auto earlyfs_size =
            static_cast<usize_t>(sys_amd64_earlyfs_image_end - sys_amd64_earlyfs_image_start);
        const auto found = platform::v1::earlyfs::find(earlyfs_begin, earlyfs_size, name);
        if (!found.valid())
            return {sys_amd64_earlyfs_image_start, sys_amd64_earlyfs_image_start};
        auto* start = const_cast<char*>(reinterpret_cast<const char*>(found.data));
        return {start, start + found.size};
    }
#else
    [[nodiscard]] inline image_view image_for_role(word_t) noexcept {
        return {sys_amd64_user_image_start, sys_amd64_user_image_end};
    }
#endif

#if CONFIG_ROOT_ONLY_BOOT
    [[nodiscard]] inline bool validate_earlyfs_image() noexcept {
        const auto* begin = reinterpret_cast<const u8*>(sys_amd64_earlyfs_image_start);
        const auto size =
            static_cast<usize_t>(sys_amd64_earlyfs_image_end - sys_amd64_earlyfs_image_start);
        if (!platform::v1::earlyfs::find(begin, size, "bin/init").valid())
            return false;
        if (!platform::v1::earlyfs::find(begin, size, "bin/memory-server").valid())
            return false;
        if (!platform::v1::earlyfs::find(begin, size, "bin/control-plane").valid())
            return false;
        if (!platform::v1::earlyfs::find(begin, size, "bin/domain-manager").valid())
            return false;
        if (platform::v1::earlyfs::find(begin, size, "no/such/entry").valid())
            return false;
        return true;
    }
#else
    [[nodiscard]] inline bool validate_earlyfs_image() noexcept {
        return true;
    }
#endif

    [[nodiscard]] inline constexpr vaddr_t user_image_end() noexcept {
        return user_code + elf64::bootstrap_size;
    }

    inline void synchronize_instruction_cache(void*, usize_t) noexcept {
        /* x86_64 has coherent icache, no sync needed */
    }

    using page_allocate_fn = elf64::page_allocate_fn;
    using page_release_fn = elf64::page_release_fn;

    struct address_space {
        memory::table_t pml4{};
        memory::table_t pdpt{};
        memory::table_t pd{};
        memory::table_t pt{};
        paddr_t image_backing[elf64::bootstrap_pages]{};
        elf64::page_permissions image_permissions[elf64::bootstrap_pages]{};
        alignas(memory::page_size) u8 stack[memory::page_size]{};
        vaddr_t image_entry{user_code};
        error_t image_status{error_t::invalid_argument};
        u16 pcid{};
        u32 pcid_generation{};
        volatile u32 active_cpu_mask{};
    };

    inline void release_image_backing(address_space& value,
                                      elf64::page_release_fn release_page) noexcept {
        for (usize_t page = 0U; page < elf64::bootstrap_pages; ++page) {
            if (value.image_backing[page] != 0U) {
                (void)release_page(value.image_backing[page]);
                value.image_backing[page] = 0U;
            }
        }
    }

    [[nodiscard]] inline error_t initialize(address_space& value, word_t role,
                                            elf64::page_allocate_fn allocate_page,
                                            elf64::page_release_fn release_page) noexcept {
        release_image_backing(value, release_page);

        asid::handle identifier{};
        const error_t pcid_result = asid::allocate(identifier);
        if (pcid_result != error_t::success)
            return pcid_result;
        value.pcid = identifier.value;
        value.pcid_generation = identifier.generation;
        value.active_cpu_mask = 0U;
        memory::build_kernel_table(value.pml4, value.pdpt, value.pd);
        memory::clear(value.pt);
        const usize_t pt_pdpt_index = static_cast<usize_t>((user_code >> 30U) & 0x1ffU);
        value.pdpt.entry[pt_pdpt_index] = memory::table_descriptor(value.pt);

        const image_view image = image_for_role(role);
        const usize_t image_size = static_cast<usize_t>(image.end - image.start);
        const elf64::result loaded = elf64::load_dynamic(
            reinterpret_cast<const u8*>(image.start), image_size, user_code, value.image_backing,
            value.image_permissions, allocate_page, release_page);
        value.image_status = loaded.status;
        value.image_entry = loaded.entry;
        if (loaded.status == error_t::success) {
            const usize_t first_index = static_cast<usize_t>((user_code >> 12U) & 0x1ffU);
            for (usize_t page = 0U; page < elf64::bootstrap_pages; ++page) {
                const auto permission = value.image_permissions[page];
                if (!permission.present)
                    continue;
                auto* page_address =
                    reinterpret_cast<void*>(static_cast<uintptr_t>(value.image_backing[page]));
                synchronize_instruction_cache(page_address, memory::page_size);
                u64 descriptor = (reinterpret_cast<u64>(page_address) & ~0xfffULL) |
                                 memory::descriptor_page | memory::descriptor_valid;
                descriptor |= memory::ap_el0_rw;
                if (!permission.writable)
                    descriptor &= ~memory::ap_el0_rw;
                if (!permission.executable)
                    descriptor |= memory::uxn;
                value.pt.entry[first_index + page] = descriptor;
            }
        }

        const u64 stack_phys = reinterpret_cast<u64>(value.stack) & ~0xfffULL;
        value.pt.entry[(user_stack_base >> 12U) & 0x1ffU] = stack_phys | memory::descriptor_page |
                                                            memory::descriptor_valid |
                                                            memory::ap_el0_rw | memory::uxn;
        return value.image_status;
    }

    inline void activate(address_space& value) noexcept {
        asid::handle identifier{value.pcid, value.pcid_generation};
        if (asid::refresh(identifier) != error_t::success)
            return;
        value.pcid = identifier.value;
        value.pcid_generation = identifier.generation;
        const u64 root = reinterpret_cast<u64>(&value.pml4) & 0x0000ffffffffffffULL;
        const u64 cr3 = root | (static_cast<u64>(value.pcid) & 0xfffULL);
        __atomic_fetch_or(&value.active_cpu_mask, 1U << arch::cpu::current_id(), __ATOMIC_RELEASE);
        __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    }

    inline void activate_kernel() noexcept {
        memory::activate(reinterpret_cast<paddr_t>(&memory::kernel_pml4));
    }

    inline void release(address_space& value, elf64::page_release_fn release_page) noexcept {
        release_image_backing(value, release_page);
        asid::handle identifier{value.pcid, value.pcid_generation};
        asid::release(identifier);
        value.pcid = 0U;
        value.pcid_generation = 0U;
    }

    inline void invalidate_pcid(u16 pcid) noexcept {
        /* INVPCID type 1: invalidate single context for the given PCID */
        struct descriptor {
            u64 pcid_val;
            u64 linear_address;
        } desc{static_cast<u64>(pcid), 0U};
        __asm__ volatile("invpcid %1, %%rax" : : "a"(1U), "m"(desc));
    }

    [[nodiscard]] inline error_t map_page(address_space& value, vaddr_t address, void* page,
                                          bool writable, bool executable, bool device,
                                          bool inner_shareable_mapping) noexcept {
        (void)device;
        (void)inner_shareable_mapping;
        if ((address & (memory::page_size - 1U)) != 0U || address < user_code ||
            address >= user_stack_base || (writable && executable))
            return error_t::invalid_argument;
        const usize_t pt_index = static_cast<usize_t>((address >> 12U) & 0x1ffU);
        if (value.pt.entry[pt_index] != 0U)
            return error_t::busy;
        u64 descriptor = (reinterpret_cast<u64>(page) & ~0xfffULL) | memory::descriptor_page |
                         memory::descriptor_valid | memory::ap_el0_rw;
        if (!writable)
            descriptor &= ~memory::ap_el0_rw;
        if (!executable)
            descriptor |= memory::uxn;
        value.pt.entry[pt_index] = descriptor;
        invalidate_pcid(value.pcid);
        return error_t::success;
    }

    [[nodiscard]] inline error_t unmap_page(address_space& value, vaddr_t address) noexcept {
        if ((address & (memory::page_size - 1U)) != 0U)
            return error_t::invalid_argument;
        const usize_t pt_index = static_cast<usize_t>((address >> 12U) & 0x1ffU);
        if (value.pt.entry[pt_index] == 0U)
            return error_t::not_found;
        value.pt.entry[pt_index] = 0U;
        invalidate_pcid(value.pcid);
        return error_t::success;
    }

    [[nodiscard]] inline vaddr_t entry(const address_space& value) noexcept {
        return value.image_entry;
    }
    [[nodiscard]] inline constexpr vaddr_t stack_top() noexcept {
        return user_stack_base + memory::page_size;
    }
} // namespace sys::arch::space
