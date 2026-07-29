#pragma once

#include <sys/platform/console.hh>
#include <sys/platform/firmware.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/memory.hh>
#include <sys/platform/timer.hh>
#include <sys/platform/v1/contract.hh>

namespace sys::platform
{
    inline constexpr const char* name = "qemu_arm64_virt";
    inline constexpr v1::version_t version{
        .major = 1U,
        .minor = 0U,
        .patch = 0U,
    };

    static_assert(v1::compatible(version));

    [[nodiscard]] inline bool certification_valid() noexcept {
        return firmware::boot_info.cpu_count == timer::maximum_cpu_count &&
               interrupt::userspace_assignable(interrupt::first_userspace_irq) &&
               !interrupt::userspace_assignable(interrupt::virtual_timer_irq) &&
               memory::ram_base != 0U && (memory::ram_base & 0xfffU) == 0U &&
               (interrupt::distributor_base & 0xffffU) == 0U &&
               (interrupt::redistributor_base & 0xffffU) == 0U;
    }
} // namespace sys::platform
