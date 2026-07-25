#pragma once
#include <sys/types.hh>

namespace sys::ipc
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::ipc
