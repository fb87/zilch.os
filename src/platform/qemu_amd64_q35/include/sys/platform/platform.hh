#pragma once

#include <sys/platform/console.hh>
#include <sys/platform/firmware.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/memory.hh>
#include <sys/platform/timer.hh>
#include <sys/platform/v1/contract.hh>

namespace sys::platform
{
    inline constexpr const char* name = "qemu_amd64_q35";
    inline constexpr v1::version_t version{
        .major = 1U,
        .minor = 0U,
        .patch = 0U,
    };

    static_assert(v1::compatible(version));

    [[nodiscard]] inline bool certification_valid() noexcept {
        return interrupt::userspace_assignable(interrupt::com1_irq + 32U) &&
               !interrupt::userspace_assignable(interrupt::virtual_timer_irq) &&
               memory::ram_base != 0U && (memory::ram_base & 0xfffU) == 0U &&
               (interrupt::lapic_base & 0xfffffU) == 0U &&
               (interrupt::ioapic_base & 0xfffffU) == 0U &&
               timer::certification_valid();
    }
} // namespace sys::platform
