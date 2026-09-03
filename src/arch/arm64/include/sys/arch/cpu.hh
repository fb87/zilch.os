#pragma once

#include <sys/types.hh>

namespace sys::arch::cpu
{
    inline void initialize_boot_cpu() noexcept {}

    [[nodiscard]] inline cpu_id_t current_id() noexcept {
        u64 value;
        __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
        return static_cast<cpu_id_t>(value & 0xffU);
    }

    inline void relax() noexcept {
        __asm__ volatile("yield" ::: "memory");
    }

    inline void wait_for_event() noexcept {
        __asm__ volatile("wfe" ::: "memory");
    }

    [[noreturn]] inline void halt() noexcept {
        for (;;) {
            wait_for_event();
        }
    }

    /* Store barrier: orders prior stores before subsequent ones become visible,
     * e.g. before reusing a page whose zeroing must be visible to any observer.
     */
    inline void store_barrier() noexcept {
        __asm__ volatile("dsb ishst" ::: "memory");
    }

    /* Full system barrier: orders all prior memory accesses against all
     * subsequent ones, system-wide.
     */
    inline void full_barrier() noexcept {
        __asm__ volatile("dsb sy" ::: "memory");
    }

    /* Full barrier plus pipeline flush, for use immediately before an
     * unconditional halt.
     */
    inline void halt_barrier() noexcept {
        __asm__ volatile("dsb sy; isb" ::: "memory");
    }

    /* Block until another CPU calls wake_parked(), for spinning on a flag
     * another CPU sets. */
    inline void park() noexcept {
        __asm__ volatile("wfe" ::: "memory");
    }

    /* Wake any CPU blocked in park(). */
    inline void wake_parked() noexcept {
        __asm__ volatile("sev" ::: "memory");
    }
} // namespace sys::arch::cpu
