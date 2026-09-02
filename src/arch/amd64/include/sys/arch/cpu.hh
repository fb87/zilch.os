#pragma once

#include <sys/types.hh>

namespace sys::arch::cpu
{
    inline void cpuid(u32 leaf, u32 subleaf, u32& eax, u32& ebx, u32& ecx, u32& edx) noexcept {
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                                  : "a"(leaf), "c"(subleaf));
    }

    [[nodiscard]] inline u32 read_apic_id() noexcept {
        u32 eax, ebx, ecx, edx;
        cpuid(1U, 0U, eax, ebx, ecx, edx);
        return (ebx >> 24U) & 0xffU;
    }

    inline void initialize_boot_cpu() noexcept {}

    [[nodiscard]] inline cpu_id_t current_id() noexcept {
        return static_cast<cpu_id_t>(read_apic_id());
    }

    inline void relax() noexcept {
        __asm__ volatile("pause" ::: "memory");
    }

    inline void wait_for_event() noexcept {
        __asm__ volatile("hlt" ::: "memory");
    }

    [[noreturn]] inline void halt() noexcept {
        __asm__ volatile("cli" ::: "memory");
        for (;;) {
            wait_for_event();
        }
    }
} // namespace sys::arch::cpu
