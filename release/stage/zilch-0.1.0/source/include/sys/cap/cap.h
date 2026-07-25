#pragma once
#include <sys/types.h>

namespace sys::cap
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::cap
