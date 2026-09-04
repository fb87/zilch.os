#pragma once

#include <sys/types.hh>

/*
 * Driver-section registration described in
 * docs/architecture/KERNEL_ARCH_PLATFORM_DECOUPLING.md.
 *
 * Every registrant, regardless of which driver or which boot stage it
 * belongs to, lands in one fixed, literally-named section
 * (".sys_driver" for init entries, ".sys_ops" for ops-vtable
 * singletons). The linker script KEEP()s those two section names
 * exactly once, permanently -- adding or removing a driver (another
 * PLAT_INIT/ARCH_INIT registration, or another SYS_OPS-tagged ops
 * struct) only ever touches that driver's own .cc file. No kernel.ld
 * edit is needed to add or remove a driver.
 *
 * What *does* require a header edit (never a linker script edit): adding
 * a genuinely new boot phase, i.e. a new `stage_t` enumerator. That's
 * rare and is a plain C++ change with no linker involvement either.
 */
namespace sys::kernel::init
{
    enum class stage_t : u16 { smmu, timer };

    struct boot_context_t {
        uintptr_t firmware_data;
        cpu_id_t cpu_id;
        bool boot_cpu;
    };

    using init_fn_t = error_t (*)(const boot_context_t*) noexcept;

    struct entry_t {
        init_fn_t fn;
        const char* name;
        stage_t stage;
    };

    extern "C" const entry_t __sys_drivers_start[];
    extern "C" const entry_t __sys_drivers_end[];

    /*
     * Iterates the *entire* combined driver table and runs only the
     * entries tagged for `stage`. One shared table instead of a
     * per-stage linker sub-range: ordering across drivers within the
     * same stage is link-order-dependent either way, and this codebase
     * does not currently rely on more than one registrant per stage.
     */
    inline void run_stage(stage_t stage, const boot_context_t& context) noexcept {
        for (const entry_t* it = __sys_drivers_start; it != __sys_drivers_end; ++it)
            if (it->stage == stage)
                (void)it->fn(&context);
    }
} // namespace sys::kernel::init

#define SYS_INIT(stage, fn)                                                                        \
    static const ::sys::kernel::init::entry_t __sys_init_##fn                                      \
        __attribute__((used, section(".sys_driver"))) = {&fn, #fn,                                 \
                                                         ::sys::kernel::init::stage_t::stage}

#define ARCH_INIT(stage, fn) SYS_INIT(stage, fn)
#define PLAT_INIT(stage, fn) SYS_INIT(stage, fn)

/*
 * Steady-state ops-vtable singletons (timer_ops_t, smmu_ops_t, ...) share
 * this one fixed section for the same reason as SYS_INIT above. clang
 * requires section() to match on every declaration of the same symbol,
 * but rejects `used` on a non-defining declaration -- so the neutral
 * contract header's `extern` declaration uses SYS_OPS_DECL (section()
 * only) and the defining instance in each platform.cc uses SYS_OPS
 * (section() + used, since that instance is the one that actually needs
 * to survive --gc-sections with no current reader).
 */
#define SYS_OPS_DECL __attribute__((section(".sys_ops")))
#define SYS_OPS __attribute__((used, section(".sys_ops")))
