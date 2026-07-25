#pragma once

#include <sys/arch/space/address_space.hh>

namespace sys::kernel::space
{
    class address_space
    {
    public:
        void initialize() noexcept { arch::space::initialize(state_); }
        void activate() noexcept { arch::space::activate(state_); }

    private:
        arch::space::address_space state_{};
    };
} // namespace sys::kernel::space
