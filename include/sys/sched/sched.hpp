#pragma once
#include <sys/types.hpp>

namespace sys::sched
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::sched
