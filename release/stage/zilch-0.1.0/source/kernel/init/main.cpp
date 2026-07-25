#include <sys/arch/current.h>
#include <sys/kernel.h>
#include <sys/platform/current.h>
#include <sys/types.h>

namespace sys::kernel
{
    namespace
    {
        void putc(char c) noexcept
        {
            platform::current::platform_ops().console.putc(c);
        }

        void puts(const char* s) noexcept
        {
            while (*s != '\0') {
                putc(*s++);
            }
        }

        void hex(u64 v) noexcept
        {
            constexpr char d[] = "0123456789abcdef";
            for (int i = 60; i >= 0; i -= 4) {
                putc(d[(v >> static_cast<unsigned>(i)) & 0xfU]);
            }
        }
    } // namespace

    [[noreturn]] void start() noexcept
    {
        const auto& p = platform::current::platform_ops();
        const auto& a = arch::current::arch_ops();
        p.console.early_init();
        puts("\nZilch L4 microkernel skeleton\n");
        puts(" platform: ");
        puts(p.name);
        puts("\n arch ABI: v");
        putc(static_cast<char>('0' + a.major));
        puts("\n word bits: 0x");
        hex(sizeof(word_t) * 8U);
        puts("\n hypervisor-ready: yes (contract only)\n status: booted\n");
        a.cpu.halt();
        for (;;) {
        }
    }
} // namespace sys::kernel

extern "C" [[noreturn]] void sys_kernel_entry() noexcept
{
    sys::kernel::start();
    for (;;) {
    }
}
