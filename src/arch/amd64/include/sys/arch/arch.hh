#pragma once

#include <sys/arch/context.hh>
#include <sys/arch/cpu.hh>
#include <sys/arch/exception.hh>
#include <sys/arch/hypervisor.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/memory.hh>
#include <sys/arch/smp.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/arch/thread/context.hh>
#include <sys/arch/thread/entry.hh>
#include <sys/arch/timer.hh>
#include <sys/arch/v1/contract.hh>

namespace sys::arch
{
    inline constexpr const char* name = "amd64";
    inline constexpr v1::version_t version{
        .major = 1U,
        .minor = 0U,
        .patch = 0U,
    };

    static_assert(v1::compatible(version));
} // namespace sys::arch
