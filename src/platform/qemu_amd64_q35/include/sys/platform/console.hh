#pragma once

#include <sys/arch/arch.hh>
#include <sys/types.hh>

namespace sys::platform::console
{
    inline constexpr u16 com1_base = 0x03f8U;

    inline void out8(u16 port, u8 value) noexcept {
        __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
    }

    [[nodiscard]]
    inline u8 in8(u16 port) noexcept {
        u8 value;
        __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    inline void initialize() noexcept {
        out8(com1_base + 1U, 0U);
        out8(com1_base + 3U, 0x80U);
        out8(com1_base + 0U, 1U);
        out8(com1_base + 1U, 0U);
        out8(com1_base + 3U, 3U);
        out8(com1_base + 2U, 0xc7U);
        out8(com1_base + 4U, 0x0bU);
    }

    inline void putc(char value) noexcept {
        while ((in8(com1_base + 5U) & 0x20U) == 0U) {
            arch::cpu::relax();
        }
        out8(com1_base, static_cast<u8>(value));
    }
} // namespace sys::platform::console
