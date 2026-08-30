#pragma once

#include <sys/types.hh>

namespace sys::guest_manifest
{
    inline constexpr u32 magic = 0x5a4d4632U; // "ZMF2"
    inline constexpr u32 version = 1U;
    inline constexpr u32 maximum_devices = 8U;
    inline constexpr u32 no_irq = 0xffffffffU;

    struct device final {
        u64 ipa{};
        u32 size{};
        u32 permissions{};
        u32 forward_irq{no_irq};
        u32 forward_trigger{}; // 0 = level, 1 = edge
    };

    struct manifest final {
        u32 header_magic{magic};
        u32 header_version{version};
        u64 ram_size{};
        u64 guest_stack{};
        u64 guest_pstate{};
        u32 device_count{};
        device devices[maximum_devices]{};
    };

    [[nodiscard]] inline bool valid(const manifest& value) noexcept {
        return value.header_magic == magic && value.header_version == version &&
               value.device_count <= maximum_devices;
    }
} // namespace sys::guest_manifest

/*
 * Defined once by whichever guest package is embedded (see
 * DOMAIN_GUEST_MANIFEST / default_manifest.cc for the fallback used when a
 * guest package supplies none). Compiled as an ordinary translation unit --
 * no binary layout matching or incbin needed, since the guest package links
 * against this same header.
 */
extern "C" const sys::guest_manifest::manifest sys_arm64_domain_guest_manifest;
