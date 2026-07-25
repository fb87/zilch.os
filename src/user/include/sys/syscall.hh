#pragma once

#include <abi/sys/v1/syscall.hh>

namespace sys
{
    [[nodiscard]] inline word_t invoke(abi::v1::Syscall number,
                                       word_t argument0 = 0U,
                                       word_t argument1 = 0U,
                                       word_t argument2 = 0U,
                                       word_t argument3 = 0U) noexcept
    {
        return sys_invoke_raw(static_cast<word_t>(number),
                              argument0,
                              argument1,
                              argument2,
                              argument3);
    }
} // namespace sys
