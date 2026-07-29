#pragma once

#include <sys/arch/v1/types.hh>
#include <sys/platform/memory.hh>
#include <sys/types.hh>

namespace sys::arch
{
    using page_attributes_t = v1::page_attributes_t;

    namespace memory
    {
        inline constexpr usize_t page_shift = 12U;
        inline constexpr usize_t page_size = 1ULL << page_shift;
        inline constexpr usize_t virtual_address_bits = 48U;
        inline constexpr bool el0_available = true;
        inline constexpr usize_t physical_address_bits = 40U;
        inline constexpr u64 descriptor_valid = 1ULL;
        inline constexpr u64 descriptor_table = 3ULL;
        inline constexpr u64 descriptor_page = 3ULL;
        inline constexpr u64 access_flag = 1ULL << 10U;
        inline constexpr u64 inner_shareable = 3ULL << 8U;
        inline constexpr u64 attr_normal = 0ULL << 2U;
        inline constexpr u64 attr_device = 1ULL << 2U;
        inline constexpr u64 ap_el1_rw = 0ULL << 6U;
        inline constexpr u64 ap_el0_rw = 1ULL << 6U;
        inline constexpr u64 ap_el1_ro = 2ULL << 6U;
        inline constexpr u64 ap_el0_ro = 3ULL << 6U;
        inline constexpr u64 pxn = 1ULL << 53U;
        inline constexpr u64 uxn = 1ULL << 54U;
        inline constexpr usize_t entries = 512U;

        struct alignas(page_size) table_t {
            u64 entry[entries];
        };
        inline table_t kernel_l0{};
        inline table_t kernel_l1{};
        inline table_t kernel_l2{};
        inline table_t kernel_identity_l2{};
        inline table_t kernel_image_l3[2]{};
        inline table_t kernel_stack_l3{};
        inline u32 kernel_identity_ready{};

        extern "C" char __cpu_stacks_start[];
        extern "C" char __hypervisor_stacks_end[];
        extern "C" char __kernel_text_start[];
        extern "C" char __kernel_text_end[];
        extern "C" char __kernel_rodata_start[];
        extern "C" char __kernel_rodata_end[];
        extern "C" char __kernel_data_start[];
        extern "C" char __kernel_data_end[];

        inline void clear(table_t& table) noexcept {
            for (usize_t i = 0U; i < entries; ++i)
                table.entry[i] = 0U;
        }

        [[nodiscard]] inline u64 table_descriptor(const table_t& table) noexcept {
            return (reinterpret_cast<u64>(&table) & ~0xfffULL) | descriptor_table;
        }

        inline void build_kernel_identity_tables() noexcept {
            if (__atomic_load_n(&kernel_identity_ready, __ATOMIC_ACQUIRE) != 0U)
                return;
            clear(kernel_identity_l2);
            clear(kernel_image_l3[0]);
            clear(kernel_image_l3[1]);
            clear(kernel_stack_l3);
            for (usize_t index = 0U; index < entries; ++index) {
                const paddr_t physical = 0x40000000ULL + index * 0x200000ULL;
                kernel_identity_l2.entry[index] = physical | descriptor_valid | access_flag |
                                                  inner_shareable | attr_normal | ap_el1_rw | uxn;
            }

            constexpr uintptr_t identity_base = 0x40000000ULL;
            constexpr usize_t image_windows = 2U;
            for (usize_t window = 0U; window < image_windows; ++window) {
                for (usize_t index = 0U; index < entries; ++index) {
                    const uintptr_t address =
                        identity_base + window * 0x200000ULL + index * page_size;
                    u64 attributes = access_flag | inner_shareable | attr_normal | uxn | pxn;
                    if (address >= reinterpret_cast<uintptr_t>(__kernel_text_start) &&
                        address < reinterpret_cast<uintptr_t>(__kernel_text_end)) {
                        attributes = access_flag | inner_shareable | attr_normal | ap_el1_ro | uxn;
                    } else if (address >= reinterpret_cast<uintptr_t>(__kernel_rodata_start) &&
                               address < reinterpret_cast<uintptr_t>(__kernel_rodata_end)) {
                        attributes =
                            access_flag | inner_shareable | attr_normal | ap_el1_ro | pxn | uxn;
                    } else {
                        attributes |= ap_el1_rw;
                    }
                    kernel_image_l3[window].entry[index] = address | descriptor_page | attributes;
                }
                kernel_identity_l2.entry[window] = table_descriptor(kernel_image_l3[window]);
            }

            const uintptr_t stack_window =
                reinterpret_cast<uintptr_t>(__cpu_stacks_start) & ~0x1fffffULL;
            const uintptr_t stack_end = reinterpret_cast<uintptr_t>(__hypervisor_stacks_end);
            if (stack_window >= 0x40000000ULL && stack_end <= stack_window + 0x200000ULL) {
                for (usize_t index = 0U; index < entries; ++index) {
                    const paddr_t physical = stack_window + index * page_size;
                    kernel_stack_l3.entry[index] = physical | descriptor_page | access_flag |
                                                   inner_shareable | attr_normal | ap_el1_rw | uxn;
                }
                constexpr usize_t cpu_count = 4U;
                constexpr usize_t slot_size = 0x10000U;
                constexpr usize_t usable_stack_size = 0x8000U;
                const uintptr_t el1_start = reinterpret_cast<uintptr_t>(__cpu_stacks_start);
                const uintptr_t el2_start =
                    reinterpret_cast<uintptr_t>(__hypervisor_stacks_end) - cpu_count * slot_size;
                for (usize_t cpu = 0U; cpu < cpu_count; ++cpu) {
                    const uintptr_t el1_guard =
                        el1_start + (cpu + 1U) * slot_size - usable_stack_size - page_size;
                    const uintptr_t el2_guard =
                        el2_start + (cpu + 1U) * slot_size - usable_stack_size - page_size;
                    kernel_stack_l3.entry[(el1_guard - stack_window) >> page_shift] = 0U;
                    kernel_stack_l3.entry[(el2_guard - stack_window) >> page_shift] = 0U;
                }
                const usize_t identity_index =
                    static_cast<usize_t>((stack_window - 0x40000000ULL) >> 21U);
                kernel_identity_l2.entry[identity_index] = table_descriptor(kernel_stack_l3);
            }
            __atomic_store_n(&kernel_identity_ready, 1U, __ATOMIC_RELEASE);
        }

        inline void build_kernel_table(table_t& l0, table_t& l1, table_t& l2) noexcept {
            build_kernel_identity_tables();
            clear(l0);
            clear(l1);
            clear(l2);
            l0.entry[0] = table_descriptor(l1);
            l1.entry[0] = table_descriptor(l2);
            l1.entry[1] = table_descriptor(kernel_identity_l2);
            l2.entry[0x08000000ULL >> 21U] = 0x08000000ULL | descriptor_valid | access_flag |
                                             attr_device | ap_el1_rw | pxn | uxn;
            l2.entry[0x09000000ULL >> 21U] = 0x09000000ULL | descriptor_valid | access_flag |
                                             attr_device | ap_el1_rw | pxn | uxn;
        }

        [[nodiscard]] inline bool kernel_stack_guards_valid() noexcept {
            if (__atomic_load_n(&kernel_identity_ready, __ATOMIC_ACQUIRE) == 0U)
                return false;
            constexpr usize_t cpu_count = 4U;
            constexpr usize_t slot_size = 0x10000U;
            constexpr usize_t usable_stack_size = 0x8000U;
            const uintptr_t stack_window =
                reinterpret_cast<uintptr_t>(__cpu_stacks_start) & ~0x1fffffULL;
            const uintptr_t el1_start = reinterpret_cast<uintptr_t>(__cpu_stacks_start);
            const uintptr_t el2_start =
                reinterpret_cast<uintptr_t>(__hypervisor_stacks_end) - cpu_count * slot_size;
            const usize_t identity_index =
                static_cast<usize_t>((stack_window - 0x40000000ULL) >> 21U);
            if (identity_index >= entries ||
                kernel_identity_l2.entry[identity_index] != table_descriptor(kernel_stack_l3))
                return false;
            for (usize_t cpu = 0U; cpu < cpu_count; ++cpu) {
                const uintptr_t el1_guard =
                    el1_start + (cpu + 1U) * slot_size - usable_stack_size - page_size;
                const uintptr_t el2_guard =
                    el2_start + (cpu + 1U) * slot_size - usable_stack_size - page_size;
                const usize_t el1_index = (el1_guard - stack_window) >> page_shift;
                const usize_t el2_index = (el2_guard - stack_window) >> page_shift;
                if (el1_index + 1U >= entries || el2_index + 1U >= entries ||
                    kernel_stack_l3.entry[el1_index] != 0U ||
                    kernel_stack_l3.entry[el2_index] != 0U ||
                    kernel_stack_l3.entry[el1_index + 1U] == 0U ||
                    kernel_stack_l3.entry[el2_index + 1U] == 0U)
                    return false;
            }
            return true;
        }

        [[nodiscard]] inline bool kernel_permissions_valid() noexcept {
            constexpr uintptr_t identity_base = 0x40000000ULL;
            for (usize_t window = 0U; window < 2U; ++window) {
                if (kernel_identity_l2.entry[window] != table_descriptor(kernel_image_l3[window]))
                    return false;
                for (usize_t index = 0U; index < entries; ++index) {
                    const uintptr_t address =
                        identity_base + window * 0x200000ULL + index * page_size;
                    const u64 descriptor = kernel_image_l3[window].entry[index];
                    const bool read_only = (descriptor & (3ULL << 6U)) == ap_el1_ro;
                    const bool executable = (descriptor & pxn) == 0U;
                    const bool text = address >= reinterpret_cast<uintptr_t>(__kernel_text_start) &&
                                      address < reinterpret_cast<uintptr_t>(__kernel_text_end);
                    const bool rodata =
                        address >= reinterpret_cast<uintptr_t>(__kernel_rodata_start) &&
                        address < reinterpret_cast<uintptr_t>(__kernel_rodata_end);
                    const bool data = address >= reinterpret_cast<uintptr_t>(__kernel_data_start) &&
                                      address < reinterpret_cast<uintptr_t>(__kernel_data_end);
                    if ((text && (!read_only || !executable)) ||
                        (rodata && (!read_only || executable)) ||
                        (data && (read_only || executable)) || (!read_only && executable))
                        return false;
                }
            }
            return true;
        }

        inline void activate(paddr_t root) noexcept {
            __asm__ volatile("dsb ishst\n\tmsr ttbr0_el1, %0\n\ttlbi vmalle1is\n\tdsb ish\n\tisb"
                             :
                             : "r"(root)
                             : "memory");
        }

        inline constexpr u64 kernel_mair = 0xffULL | (0x04ULL << 8U);
        inline constexpr u64 kernel_tcr =
            16ULL | (1ULL << 8U) | (1ULL << 10U) | (3ULL << 12U) | (1ULL << 23U) | (2ULL << 32U);
        inline constexpr u64 kernel_sctlr_required =
            1ULL | (1ULL << 2U) | (1ULL << 12U) | (1ULL << 19U);

        inline void initialize_cpu() noexcept {
            __asm__ volatile("msr mair_el1, %0\n\tmsr tcr_el1, %1\n\tisb"
                             :
                             : "r"(kernel_mair), "r"(kernel_tcr)
                             : "memory");
            activate(reinterpret_cast<paddr_t>(&kernel_l0));
            u64 sctlr;
            __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
            sctlr |= kernel_sctlr_required;
            __asm__ volatile("msr sctlr_el1, %0\n\tisb" : : "r"(sctlr) : "memory");
            /*
             * The kernel has no implicit privileged access to EL0 mappings.
             * User-memory access must eventually pass through explicit copy
             * primitives which temporarily relax PAN; UAO stays disabled.
             */
            u64 features{};
            __asm__ volatile("mrs %0, id_aa64mmfr1_el1" : "=r"(features));
            if (((features >> 20U) & 0xfU) != 0U)
                __asm__ volatile(".inst 0xd500419f\n\tisb" ::: "memory"); // MSR PAN, #1
            if (((features >> 4U) & 0xfU) != 0U)
                __asm__ volatile(".inst 0xd500407f\n\tisb" ::: "memory"); // MSR UAO, #0
        }

        [[nodiscard]] inline bool privilege_protection_enabled() noexcept {
            u64 features{};
            u64 pan{};
            u64 uao{};
            __asm__ volatile("mrs %0, id_aa64mmfr1_el1" : "=r"(features));
            if (((features >> 20U) & 0xfU) != 0U)
                __asm__ volatile("mrs %0, S3_0_C4_C2_3" : "=r"(pan));
            if (((features >> 4U) & 0xfU) != 0U)
                __asm__ volatile("mrs %0, S3_0_C4_C2_4" : "=r"(uao));
            return (((features >> 20U) & 0xfU) == 0U || (pan & 1U) != 0U) &&
                   (((features >> 4U) & 0xfU) == 0U || (uao & 1U) == 0U);
        }

        [[nodiscard]] inline bool architectural_controls_valid() noexcept {
            u64 mair{};
            u64 tcr{};
            u64 sctlr{};
            __asm__ volatile("mrs %0, mair_el1\n\tmrs %1, tcr_el1\n\tmrs %2, sctlr_el1"
                             : "=r"(mair), "=r"(tcr), "=r"(sctlr));
            constexpr u64 endian_bits = (1ULL << 25U) | (1ULL << 24U);
            return mair == kernel_mair && tcr == kernel_tcr &&
                   (sctlr & kernel_sctlr_required) == kernel_sctlr_required &&
                   (sctlr & endian_bits) == 0U;
        }

        inline void initialize() noexcept {
            build_kernel_table(kernel_l0, kernel_l1, kernel_l2);
            initialize_cpu();
        }

        [[nodiscard]] inline error_t map_page(paddr_t, vaddr_t, paddr_t,
                                              page_attributes_t) noexcept {
            return error_t::unsupported;
        }

        inline void invalidate_tlb_all() noexcept {
            __asm__ volatile("dsb ishst\n\ttlbi vmalle1is\n\tdsb ish\n\tisb" ::: "memory");
        }
    } // namespace memory
} // namespace sys::arch
