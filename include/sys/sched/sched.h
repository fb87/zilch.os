#pragma once
#include <sys/types.h>

namespace sys::sched
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::sched
