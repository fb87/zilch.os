#pragma once
#include <sys/types.hpp>

namespace sys::cap
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::cap
