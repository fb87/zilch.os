#pragma once
#include <sys/types.hh>

namespace sys::cap
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::cap
