#pragma once

#include <sys/arch/hardening.hh>
#include <sys/arch/memory.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/types.hh>

namespace sys::arch::user_access
{
    [[nodiscard]] inline bool valid_range(const space::address_space& space, vaddr_t address,
                                          usize_t size, bool write) noexcept {
        if (size == 0U)
            return space::in_user_window(address);
        if (address >= space::user_window_end || size > space::user_window_end - address)
            return false;
        const vaddr_t final = address + size - 1U;
        if (!space::in_user_window(address) || !space::in_user_window(final))
            return false;
        /*
         * Walks descriptors through space::page_descriptor() rather than
         * indexing one embedded table. The old form assumed every user page
         * lived in the single L2 block holding user_code and rejected
         * anything else outright, which silently became wrong the moment a
         * process could map a heap into a second block -- a range there
         * would have been refused, not mis-validated, but refusing a legal
         * copy is still a bug. Unmapped blocks return a zero descriptor and
         * fail the valid-bit test below exactly like an unmapped page.
         */
        for (vaddr_t page = address & ~(memory::page_size - 1U); page <= final;) {
            const u64 descriptor = space::page_descriptor(space, page);
            if ((descriptor & memory::descriptor_valid) == 0U)
                return false;
            const u64 access = descriptor & (3ULL << 6U);
            if (access != memory::ap_el0_rw && access != memory::ap_el0_ro)
                return false;
            if (write && access != memory::ap_el0_rw)
                return false;
            if (final - page < memory::page_size)
                break;
            page += memory::page_size;
        }
        hardening::speculation_barrier();
        return true;
    }

    [[nodiscard]] inline error_t copy_from_user(const space::address_space& space,
                                                void* destination, vaddr_t source,
                                                usize_t size) noexcept {
        if (destination == nullptr || !valid_range(space, source, size, false))
            return error_t::invalid_argument;
        auto* output = reinterpret_cast<u8*>(destination);
        for (usize_t index = 0U; index < size; ++index) {
            u64 value{};
            __asm__ volatile("ldtrb %w0, [%1]" : "=r"(value) : "r"(source + index) : "memory");
            output[index] = static_cast<u8>(value);
        }
        return error_t::success;
    }

    [[nodiscard]] inline error_t copy_to_user(const space::address_space& space,
                                              vaddr_t destination, const void* source,
                                              usize_t size) noexcept {
        if (source == nullptr || !valid_range(space, destination, size, true))
            return error_t::invalid_argument;
        const auto* input = reinterpret_cast<const u8*>(source);
        for (usize_t index = 0U; index < size; ++index) {
            const u64 value = input[index];
            __asm__ volatile("sttrb %w0, [%1]" : : "r"(value), "r"(destination + index) : "memory");
        }
        return error_t::success;
    }
} // namespace sys::arch::user_access
