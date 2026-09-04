#pragma once

#include <sys/kernel/init/init.hh>
#include <sys/platform/v1/types.hh>
#include <sys/types.hh>

/*
 * Steady-state ops-vtable contract described in
 * docs/architecture/KERNEL_ARCH_PLATFORM_DECOUPLING.md. Exactly one
 * platform is linked into a given image, so this is a single well-known
 * extern symbol (mechanism 2), placed via SYS_OPS in the generic
 * ".sys_ops" section every ops-vtable singleton shares -- not a
 * linker-section registry (mechanism 1 is for genuine multi-registrant
 * cases, e.g. sys/kernel/init's driver table).
 *
 * Only the members kernel.hh's boot sequence reads are covered so far:
 * the many steady-state sys::platform::timer::* call sites in
 * scheduler.hh/ipc.hh/control.hh/interrupt.hh/arch.cc are intentionally
 * left calling the free functions directly -- converting kernel/IPC hot
 * paths to indirect calls is a separate, more careful pass, not part of
 * this pilot.
 */
namespace sys::platform::v1
{
    struct timer_ops_t {
        u32 ticks_per_second;
        void (*initialize)() noexcept;
        u64 (*ticks)(cpu_id_t cpu_id) noexcept;
        bool (*certification_valid)() noexcept;
    };
} // namespace sys::platform::v1

extern "C" SYS_OPS_DECL const sys::platform::v1::timer_ops_t sys_platform_timer_ops;
