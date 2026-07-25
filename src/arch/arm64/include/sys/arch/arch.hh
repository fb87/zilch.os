#pragma once

#include <sys/arch/v1/contract.hh>
#include <sys/arch/context.hh>
#include <sys/arch/cpu.hh>
#include <sys/arch/hypervisor.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/memory.hh>
#include <sys/arch/exception.hh>
#include <sys/arch/smp.hh>
#include <sys/arch/timer.hh>

namespace sys::arch
{
    inline constexpr const char* name = "arm64";
    inline constexpr v1::version_t version{
        .major = 1U,
        .minor = 0U,
        .patch = 0U,
    };

    static_assert(v1::compatible(version));
} // namespace sys::arch
