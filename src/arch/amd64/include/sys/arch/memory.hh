#pragma once

#include <sys/arch/v1/types.hh>
#include <sys/types.hh>

namespace sys::arch
{
    using page_attributes_t = v1::page_attributes_t;

    namespace memory
    {
        inline constexpr usize_t page_shift = 12U;
        inline constexpr usize_t page_size = 1ULL << page_shift;
        inline constexpr usize_t virtual_address_bits = 48U;
        inline constexpr usize_t physical_address_bits = 52U;
        inline constexpr bool el0_available = false;
        inline constexpr usize_t entries = 512U;
        /* x86_64 PTE format (4-level paging):
         * Bits [11:0]: flags (PRESENT, WRITE, USER, etc.)
         * Bits [51:12]: physical address (40 bits on qemu, up to 52 bits on real HW)
         * Bit 63: XD (execute-disable / NX)
         */
        inline constexpr u64 descriptor_valid = 1ULL;  /* PRESENT bit (bit 0) */
        inline constexpr u64 descriptor_page = 1ULL;   /* PRESENT for leaf entries */
        inline constexpr u64 descriptor_table = 1ULL;  /* PRESENT for table entries */
        inline constexpr u64 access_flag = 1ULL << 5U; /* ACCESSED bit (bit 5), optional */
        inline constexpr u64 inner_shareable = 0ULL;   /* not applicable to x86 */
        inline constexpr u64 attr_normal = 0ULL;       /* not applicable to x86 basic paging */
        inline constexpr u64 ap_el0_ro = 1ULL << 2U;   /* USER bit (bit 2), no WRITE bit */
        inline constexpr u64 ap_el0_rw = (1ULL << 2U) | (1ULL << 1U); /* USER + WRITE bits */
        inline constexpr u64 pxn = 0ULL;        /* x86 NX is global, not per-privilege */
        inline constexpr u64 uxn = 1ULL << 63U; /* NX bit (bit 63) */

        struct alignas(page_size) table_t {
            u64 entry[entries];
        };

        inline table_t kernel_pml4{};
        inline table_t kernel_pdpt{};
        inline table_t kernel_pd{};

        inline void clear(table_t& table) noexcept {
            for (usize_t i = 0U; i < entries; ++i)
                table.entry[i] = 0U;
        }

        [[nodiscard]] inline u64 table_descriptor(const table_t& table) noexcept {
            return (reinterpret_cast<u64>(&table) & ~0xfffULL) | descriptor_table;
        }

        inline void build_kernel_table(table_t& pml4, table_t& pdpt, table_t& pd) noexcept {
            clear(pml4);
            clear(pdpt);
            clear(pd);

            /* Install PDPT pointer in PML4 entry 0 (kernel lower-half mapping) */
            pml4.entry[0] = table_descriptor(pdpt);

            /* Install PD pointer in PDPT entry 0 */
            pdpt.entry[0] = table_descriptor(pd);

            /* Map kernel identity region (first 1 GiB) with 2 MiB huge pages.
             * Using kernel-mode read-write (PRESENT | WRITE, no USER bit).
             */
            for (usize_t i = 0U; i < 512U; ++i) {
                const paddr_t phys = i * (2ULL << 20U); /* 2 MiB pages */
                /* Bits: PRESENT(1) | WRITE(2) | PS(128=0x80) | ACCESSED(32=0x20) */
                pd.entry[i] = phys | 0x183U;
            }
        }

        inline void activate(paddr_t pml4_phys) noexcept {
            __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
        }

        inline void initialize() noexcept {
            /* For now, we keep using the temporary tables from boot/start.S; this just
             * verifies the table building logic works. In later phases, we'll switch to
             * using these real tables.
             */
            build_kernel_table(kernel_pml4, kernel_pdpt, kernel_pd);
        }
        inline void initialize_cpu() noexcept {}

        [[nodiscard]] inline error_t map_page([[maybe_unused]] paddr_t pml4_phys,
                                              [[maybe_unused]] vaddr_t vaddr,
                                              [[maybe_unused]] paddr_t paddr,
                                              [[maybe_unused]] page_attributes_t perms) noexcept {
            /* For Phase 2, this is a stub. Full paging support comes later. */
            return error_t::unsupported;
        }

        inline void invalidate_tlb_all() noexcept {
            word_t cr3;
            __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
            __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
        }
    } // namespace memory
} // namespace sys::arch
