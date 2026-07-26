#pragma once

#include <sys/platform/v1/types.hh>
#include <sys/types.hh>

namespace sys::platform::firmware
{
    inline constexpr v1::boot_info_t boot_info{
        .firmware_data = 0U,
        .earlyfs_base = 0U,
        .earlyfs_size = 0U,
        .boot_cpu = 0U,
        .cpu_count = 4U,
    };

    inline constexpr u64 psci_cpu_on_64 = 0xc4000003ULL;

    // QEMU virt exposes PSCI through SMC when the guest boots with EL2.
    // HVC is reserved for guests that do not have EL2.
    enum class psci_result : s64 {
        success = 0,
        not_supported = -1,
        invalid_parameters = -2,
        denied = -3,
        already_on = -4,
        on_pending = -5,
        internal_failure = -6,
        not_present = -7,
        disabled = -8,
        invalid_address = -9,
    };

    [[nodiscard]] inline error_t start_cpu(cpu_id_t target_cpu, uintptr_t entry,
                                           uintptr_t context) noexcept {
        register u64 x0 __asm__("x0") = psci_cpu_on_64;
        register u64 x1 __asm__("x1") = static_cast<u64>(target_cpu);
        register u64 x2 __asm__("x2") = static_cast<u64>(entry);
        register u64 x3 __asm__("x3") = static_cast<u64>(context);
        __asm__ volatile("smc #0"
                         : "+r"(x0)
                         : "r"(x1), "r"(x2), "r"(x3)
                         : "x4", "x5", "x6", "x7", "memory");

        const auto result = static_cast<psci_result>(static_cast<s64>(x0));
        switch (result) {
            case psci_result::success:
            case psci_result::already_on:
                return error_t::success;
            case psci_result::not_supported:
                return error_t::unsupported;
            case psci_result::invalid_parameters:
            case psci_result::invalid_address:
                return error_t::invalid_argument;
            case psci_result::denied:
            case psci_result::disabled:
                return error_t::denied;
            case psci_result::on_pending:
                return error_t::busy;
            case psci_result::not_present:
                return error_t::not_found;
            case psci_result::internal_failure:
            default:
                return error_t::denied;
        }
    }
} // namespace sys::platform::firmware
