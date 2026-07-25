#pragma once

#include <sys/arch/current.hpp>
#include <sys/platform/current.hpp>
#include <sys/printk.hpp>

namespace sys::kernel
{
    using namespace arch::current;
    using namespace platform::current;

    void init() noexcept {
        platform_ops().console.early_init();
    }

    [[noreturn]] void panic() noexcept {
        printk("Panic!");

        arch_ops().cpu.halt();
        for (;;) {
            arch_ops().cpu.relax();
        }
    }

    [[noreturn]] void start() noexcept {
        sys::kernel::init();

        printk("\n");
        printk("     ███████╗██╗██╗      ██████╗██╗  ██╗     ██████╗ ███████╗\n");
        printk("     ╚══███╔╝██║██║     ██╔════╝██║  ██║    ██╔═══██╗██╔════╝\n");
        printk("       ███╔╝ ██║██║     ██║     ███████║    ██║   ██║███████╗\n");
        printk("      ███╔╝  ██║██║     ██║     ██╔══██║    ██║   ██║╚════██║\n");
        printk("     ███████╗██║███████╗╚██████╗██║  ██║    ╚██████╔╝███████║\n");
        printk("     ╚══════╝╚═╝╚══════╝ ╚═════╝╚═╝  ╚═╝     ╚═════╝ ╚══════╝\n");
        printk("                                                (C)2026 Zilch\n");
        printk("       - Platform:         %s\n", platform_ops().name);
        printk("       - Arch ABI:         v%d\n", arch_ops().major);
        printk("       - Word bits:        %dbit\n", (u8)sizeof(word_t) * 8U);
        printk("       - Hypervisor-ready: yes\n");
        printk("       - Status:           booted, CPU%d online\n", arch_ops().cpu.current_cpu());
        printk("\n");

        // never return to main, just loop forever. The kernel is now running and will never return
        // to main.
        sys::kernel::panic();
    }
} // namespace sys::kernel

extern "C" [[noreturn]] void sys_kernel_entry() noexcept;
