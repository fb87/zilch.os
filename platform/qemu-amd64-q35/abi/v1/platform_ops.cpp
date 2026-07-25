#include <sys/platform/v1/platform_ops.h>

namespace sys::platform::v1
{
    namespace
    {
        void outb(u16 p, u8 v) noexcept {
            __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p));
        }

        u8 inb(u16 p) noexcept {
            u8 v;
            __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
            return v;
        }

        void init() noexcept {
            outb(0x3f9, 0);
            outb(0x3fb, 0x80);
            outb(0x3f8, 1);
            outb(0x3f9, 0);
            outb(0x3fb, 3);
            outb(0x3fa, 0xc7);
            outb(0x3fc, 0x0b);
        }

        void putc(char c) noexcept {
            while ((inb(0x3fd) & 0x20U) == 0U) {
            }
            outb(0x3f8, static_cast<u8>(c));
        }

        Error irq_init() noexcept {
            return Error::unsupported;
        }

        irq_id_t ack() noexcept {
            return 0U;
        }

        void complete(irq_id_t) noexcept {}

        const BootInfo info{nullptr, 0, 0, 0, 1};

        const BootInfo* boot() noexcept {
            return &info;
        }

        const PlatformOps ops{1,
                              0,
                              0,
                              sizeof(PlatformOps),
                              "qemu-amd64-q35",
                              {init, putc},
                              {irq_init, ack, complete},
                              boot};
    } // namespace

    const PlatformOps& platform_ops() noexcept {
        return ops;
    }
} // namespace sys::platform::v1
