#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/types.hh>

namespace sys::arch::user_access
{
    /*
     * amd64 has a real per-process page table (space::address_space::pt), but
     * no SMAP-safe unprivileged load/store equivalent to arm64's LDTRB/STTRB
     * wired up yet, and no real syscall entry (Phase 7) exercising this path
     * -- arch::syscall::is_user_syscall() is unconditionally false on this
     * platform, so nothing calls into user_access:: here today. Honestly
     * report unsupported rather than guess at an unverified implementation.
     */
    [[nodiscard]] inline bool valid_range(const space::address_space&, vaddr_t, usize_t,
                                          bool) noexcept {
        return false;
    }

    [[nodiscard]] inline error_t copy_from_user(const space::address_space&, void*, vaddr_t,
                                                usize_t) noexcept {
        return error_t::unsupported;
    }

    [[nodiscard]] inline error_t copy_to_user(const space::address_space&, vaddr_t, const void*,
                                              usize_t) noexcept {
        return error_t::unsupported;
    }
} // namespace sys::arch::user_access
