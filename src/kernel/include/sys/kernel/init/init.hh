#pragma once

#include <sys/types.hh>

/*
 * Pilot of the linker-section init registration described in
 * docs/architecture/KERNEL_ARCH_PLATFORM_DECOUPLING.md. Only the `timer`
 * stage is wired up so far -- this validates the linker mechanics
 * (section placement, KEEP(), boundary symbols, .rodata placement)
 * against one subsystem before any other stage is added.
 */
namespace sys::kernel::init
{
    struct boot_context_t {
        uintptr_t firmware_data;
        cpu_id_t cpu_id;
        bool boot_cpu;
    };

    using init_fn_t = error_t (*)(const boot_context_t*) noexcept;

    struct entry_t {
        init_fn_t fn;
        const char* name;
    };

    inline void run_stage(const entry_t* begin, const entry_t* end,
                          const boot_context_t& context) noexcept {
        for (const entry_t* it = begin; it != end; ++it)
            (void)it->fn(&context);
    }
} // namespace sys::kernel::init

#define SYS_INIT(stage, fn)                                                                        \
    static const ::sys::kernel::init::entry_t __sys_init_##fn                                      \
        __attribute__((used, section(".sys_init_" #stage))) = {fn, #fn}

#define ARCH_INIT(stage, fn) SYS_INIT(stage, fn)
#define PLAT_INIT(stage, fn) SYS_INIT(stage, fn)
