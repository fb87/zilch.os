#pragma once

#include <sys/types.hh>

/*
 * See cpu.hh's header comment: this tree is a hardware-free stand-in used
 * only to let tests/host/kernel_logic.cc compile and run natively on
 * whatever machine invokes tools/verification/run_host_kernel_logic.sh.
 *
 * Unlike cpu.hh's current_id()/relax(), a real arch's counter()/frequency()
 * would actually have compiled and run correctly on either a native x86_64
 * or aarch64 host -- amd64's `rdtsc` and arm64's `cntvct_el0` are both
 * unprivileged. This still gets its own hardware-free implementation for
 * consistency with cpu.hh and because the value returned is never checked
 * for realism by anything this test exercises, only used (if at all) as an
 * opaque, monotonically-useful-enough tick source.
 */
namespace sys::arch::timer
{
    inline u64 host_counter_value = 0U;

    [[nodiscard]] inline u64 counter() noexcept {
        return ++host_counter_value;
    }

    [[nodiscard]] inline u64 frequency() noexcept {
        return 0U;
    }
} // namespace sys::arch::timer
