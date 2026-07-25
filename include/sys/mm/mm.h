#pragma once
#include <sys/types.h>

namespace sys::mm
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::mm
