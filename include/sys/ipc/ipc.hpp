#pragma once
#include <sys/types.hpp>

namespace sys::ipc
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::ipc
