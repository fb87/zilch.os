#pragma once
#include <sys/platform/v1/types.h>

namespace sys::platform::v1
{
    struct ConsoleOps final
    {
        void (*early_init)() noexcept;
        void (*putc)(char) noexcept;
    };

    struct InterruptControllerOps final
    {
        Error (*initialize)() noexcept;
        irq_id_t (*acknowledge)() noexcept;
        void (*complete)(irq_id_t) noexcept;
    };

    struct PlatformOps final
    {
        u16 major, minor, patch, size;
        const char* name;
        ConsoleOps console;
        InterruptControllerOps interrupt;
        const BootInfo* (*boot_info)() noexcept;
    };

    [[nodiscard]] const PlatformOps& platform_ops() noexcept;
} // namespace sys::platform::v1
