#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/memory.hh>
#include <sys/types.hh>

namespace sys::arch::space
{
    inline constexpr bool user_available = true;
#if CONFIG_ROOT_ONLY_BOOT
    inline constexpr vaddr_t user_code = 0x20000000ULL;
    inline constexpr vaddr_t user_stack_base = 0x20010000ULL;
#else
    inline constexpr vaddr_t user_code = 0x10000000ULL;
    inline constexpr vaddr_t user_stack_base = 0x10010000ULL;
#endif

    // TTBR0 currently preserves the kernel identity block at 0x40000000.
    // User mappings must remain below that L1 block until kernel mappings
    // move to TTBR1_EL1.
    inline constexpr vaddr_t kernel_identity_base = 0x40000000ULL;
    static_assert(user_code < kernel_identity_base);
    static_assert(user_stack_base < kernel_identity_base);

    extern "C" char sys_arm64_user_image_start[];
    extern "C" char sys_arm64_user_image_end[];
    extern "C" char sys_arm64_memory_server_image_start[];
    extern "C" char sys_arm64_memory_server_image_end[];
    extern "C" char sys_arm64_pager_client_image_start[];
    extern "C" char sys_arm64_pager_client_image_end[];

    inline constexpr word_t memory_server_image_role = 0x100U;
    inline constexpr word_t pager_client_image_role_base = 0x101U;

    struct image_view {
        char* start;
        char* end;
    };

    [[nodiscard]] inline image_view image_for_role(word_t role) noexcept {
        if (role == memory_server_image_role)
            return {sys_arm64_memory_server_image_start, sys_arm64_memory_server_image_end};
        if (role == pager_client_image_role_base || role == pager_client_image_role_base + 1U)
            return {sys_arm64_pager_client_image_start, sys_arm64_pager_client_image_end};
        return {sys_arm64_user_image_start, sys_arm64_user_image_end};
    }

    [[nodiscard]] inline usize_t user_image_size() noexcept {
        return static_cast<usize_t>(sys_arm64_user_image_end - sys_arm64_user_image_start);
    }

    [[nodiscard]] inline usize_t user_image_pages() noexcept {
        return (user_image_size() + memory::page_size - 1U) / memory::page_size;
    }

    [[nodiscard]] inline vaddr_t user_image_end() noexcept {
        return user_code + static_cast<vaddr_t>(user_image_pages()) * memory::page_size;
    }

    struct address_space {
        memory::table_t l0{};
        memory::table_t l1{};
        memory::table_t l2{};
        memory::table_t l3{};
        alignas(memory::page_size) u8 stack[memory::page_size]{};
        u16 asid{};
        volatile u32 active_cpu_mask{};
    };

    inline void initialize(address_space& value, u16 asid, word_t role) noexcept {
        value.asid = asid;
        value.active_cpu_mask = 0U;
        memory::build_kernel_table(value.l0, value.l1, value.l2);
        const usize_t code_l2 = static_cast<usize_t>((user_code >> 21U) & 0x1ffU);
        value.l2.entry[code_l2] = memory::table_descriptor(value.l3);

        const image_view image = image_for_role(role);
        const u64 image_phys = reinterpret_cast<u64>(image.start) & ~0xfffULL;
        const usize_t image_size = static_cast<usize_t>(image.end - image.start);
        const usize_t image_pages = (image_size + memory::page_size - 1U) / memory::page_size;
        const usize_t first_index = static_cast<usize_t>((user_code >> 12U) & 0x1ffU);
        for (usize_t page = 0U; page < image_pages; ++page) {
            const usize_t index = first_index + page;
            if (index >= 512U)
                break;
            value.l3.entry[index] = (image_phys + page * memory::page_size) |
                                    memory::descriptor_page | memory::access_flag |
                                    memory::inner_shareable | memory::attr_normal |
                                    memory::ap_el0_ro;
        }

        const u64 stack_phys = reinterpret_cast<u64>(value.stack) & ~0xfffULL;
        value.l3.entry[(user_stack_base >> 12U) & 0x1ffU] =
            stack_phys | memory::descriptor_page | memory::access_flag | memory::inner_shareable |
            memory::attr_normal | memory::ap_el0_rw | memory::pxn | memory::uxn;
    }

    inline void activate(address_space& value) noexcept {
        const u64 root = reinterpret_cast<u64>(&value.l0) & 0x0000ffffffffffffULL;
        const u64 ttbr = root | (static_cast<u64>(value.asid) << 48U);
        __atomic_fetch_or(&value.active_cpu_mask, 1U << arch::cpu::current_id(), __ATOMIC_RELEASE);
        __asm__ volatile("dsb ishst\n\tmsr ttbr0_el1, %0\n\tisb" : : "r"(ttbr) : "memory");
    }

    inline void invalidate_asid(u16 asid) noexcept {
        const u64 operand = static_cast<u64>(asid) << 48U;
        __asm__ volatile("dsb ishst\n\ttlbi aside1is, %0\n\tdsb ish\n\tisb"
                         :
                         : "r"(operand)
                         : "memory");
    }

    [[nodiscard]] inline error_t map_page(address_space& value, vaddr_t address, void* page,
                                          bool writable, bool executable) noexcept {
        if ((address & (memory::page_size - 1U)) != 0U || address < user_code ||
            address >= user_stack_base || (writable && executable))
            return error_t::invalid_argument;
        const usize_t l2_index = static_cast<usize_t>((address >> 21U) & 0x1ffU);
        if (l2_index != static_cast<usize_t>((user_code >> 21U) & 0x1ffU))
            return error_t::unsupported;
        const usize_t l3_index = static_cast<usize_t>((address >> 12U) & 0x1ffU);
        if (value.l3.entry[l3_index] != 0U)
            return error_t::busy;
        u64 descriptor = (reinterpret_cast<u64>(page) & ~0xfffULL) | memory::descriptor_page |
                         memory::access_flag | memory::inner_shareable | memory::attr_normal;
        descriptor |= writable ? memory::ap_el0_rw : memory::ap_el0_ro;
        if (!executable)
            descriptor |= memory::pxn | memory::uxn;
        value.l3.entry[l3_index] = descriptor;
        invalidate_asid(value.asid);
        return error_t::success;
    }

    [[nodiscard]] inline error_t unmap_page(address_space& value, vaddr_t address) noexcept {
        if ((address & (memory::page_size - 1U)) != 0U)
            return error_t::invalid_argument;
        const usize_t l3_index = static_cast<usize_t>((address >> 12U) & 0x1ffU);
        if (value.l3.entry[l3_index] == 0U)
            return error_t::not_found;
        value.l3.entry[l3_index] = 0U;
        invalidate_asid(value.asid);
        return error_t::success;
    }

    [[nodiscard]] inline constexpr vaddr_t entry() noexcept {
        return user_code;
    }
    [[nodiscard]] inline constexpr vaddr_t stack_top() noexcept {
        return user_stack_base + memory::page_size;
    }
} // namespace sys::arch::space
