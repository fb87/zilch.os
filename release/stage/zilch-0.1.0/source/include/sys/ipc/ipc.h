#pragma once
#include <sys/types.h>

namespace sys::ipc
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::ipc
