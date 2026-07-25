#pragma once
#include <sys/types.hh>

namespace sys::sched
{
    [[nodiscard]] Error initialize() noexcept;
} // namespace sys::sched
