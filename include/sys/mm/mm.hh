#pragma once
#include <sys/types.hh>

namespace sys::mm
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::mm
