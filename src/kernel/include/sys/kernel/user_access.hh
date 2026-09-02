#pragma once

#include <sys/arch/hardening.hh>
#include <sys/arch/memory.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/types.hh>

namespace sys::kernel::user_access
{
#if defined(__aarch64__)
    [[nodiscard]] inline bool valid_range(const arch::space::address_space& space, vaddr_t address,
                                          usize_t size, bool write) noexcept {
        if (size == 0U)
            return address >= arch::space::user_code && address < arch::space::kernel_identity_base;
        if (address >= arch::space::kernel_identity_base ||
            size > arch::space::kernel_identity_base - address)
            return false;
        const vaddr_t final = address + size - 1U;
        if (address < arch::space::user_code || final >= arch::space::kernel_identity_base)
            return false;
        const usize_t expected_l2 = static_cast<usize_t>((arch::space::user_code >> 21U) & 0x1ffU);
        if (((address >> 21U) & 0x1ffU) != expected_l2 || ((final >> 21U) & 0x1ffU) != expected_l2)
            return false;
        for (vaddr_t page = address & ~(arch::memory::page_size - 1U); page <= final;) {
            const u64 descriptor = space.l3.entry[(page >> 12U) & 0x1ffU];
            if ((descriptor & arch::memory::descriptor_valid) == 0U)
                return false;
            const u64 access = descriptor & (3ULL << 6U);
            if (access != arch::memory::ap_el0_rw && access != arch::memory::ap_el0_ro)
                return false;
            if (write && access != arch::memory::ap_el0_rw)
                return false;
            if (final - page < arch::memory::page_size)
                break;
            page += arch::memory::page_size;
        }
        arch::hardening::speculation_barrier();
        return true;
    }

    [[nodiscard]] inline error_t copy_from_user(const arch::space::address_space& space,
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

    [[nodiscard]] inline error_t copy_to_user(const arch::space::address_space& space,
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
#endif /* __aarch64__ */
} // namespace sys::kernel::user_access
