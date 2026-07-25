#pragma once

#include <sys/arch/arch.hh>
#include <sys/types.hh>

namespace sys::platform::console
{
    inline constexpr uintptr_t uart_base = 0x09000000ULL;
    inline constexpr uintptr_t data_offset = 0x00U;
    inline constexpr uintptr_t flag_offset = 0x18U;
    inline constexpr u32 transmit_fifo_full = 1U << 5U;

    inline void initialize() noexcept {}

    inline void putc(char value) noexcept {
        auto* data = reinterpret_cast<volatile u32*>(uart_base + data_offset);
        auto* flags = reinterpret_cast<volatile u32*>(uart_base + flag_offset);

        while ((*flags & transmit_fifo_full) != 0U) {
            arch::cpu::relax();
        }
        *data = static_cast<u32>(static_cast<u8>(value));
    }
} // namespace sys::platform::console
