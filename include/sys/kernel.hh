#pragma once

namespace sys::kernel
{
    [[noreturn]] void start() noexcept;
} // namespace sys::kernel

extern "C" [[noreturn]] void sys_kernel_entry() noexcept;
