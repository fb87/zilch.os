#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/types.hh>

namespace sys::kernel::space
{
    struct address_space
    {
        arch::space::address_space native{};

        inline void initialize(u16 asid) noexcept
        {
            arch::space::initialize(native, asid);
        }

        inline void activate() noexcept
        {
            arch::space::activate(native);
        }
    };
} // namespace sys::kernel::space
