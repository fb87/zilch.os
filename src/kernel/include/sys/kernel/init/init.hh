#pragma once

#include <sys/kernel/printk.hh>
#include <sys/types.hh>

/*
 * Driver-section registration described in
 * docs/architecture/KERNEL_ARCH_PLATFORM_DECOUPLING.md.
 *
 * Every registrant, regardless of which driver it is, lands in one
 * fixed, literally-named section (".sys_driver" for init entries,
 * ".sys_ops" for ops-vtable singletons). The linker script KEEP()s those
 * two section names exactly once, permanently -- adding or removing a
 * driver (another PLAT_INIT/ARCH_INIT registration, or another
 * SYS_OPS-tagged ops struct) only ever touches that driver's own .cc
 * file. No kernel.ld edit is needed to add or remove a driver.
 *
 * `stage_t` is deliberately *not* named after specific drivers (it used
 * to have `smmu`/`timer` enumerators, which meant kernel.hh had to name
 * every driver it wanted to run -- exactly the coupling this mechanism
 * is supposed to remove). It names the two structural cardinality
 * classes every driver's init function falls into, which kernel.hh
 * legitimately does need to know about to orchestrate boot regardless of
 * which drivers exist:
 *
 *   - `once`:   runs exactly once, only on the boot CPU, during start().
 *   - `percpu`: runs once per CPU, in both start() and start_secondary().
 *
 * Adding a new driver that fits one of these two categories -- which
 * covers every driver so far -- needs no kernel.hh or init.hh change at
 * all: just a PLAT_INIT/ARCH_INIT call in the driver's own .cc file
 * tagging it `once` or `percpu`. Only a genuinely new *ordering or
 * cardinality* requirement that neither category expresses would need a
 * new enumerator here and a new call site in kernel.hh -- a rare,
 * legitimate boot-policy decision, still never a linker script change.
 */
namespace sys::kernel::init
{
    enum class stage_t : u16 { once, percpu };

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
     * entries tagged for `stage`, logging each by its own registered
     * name -- generically, so kernel.hh's call sites never need to name
     * a specific driver in a log message either. Ordering across drivers
     * within the same stage is link-order-dependent; this codebase does
     * not currently rely on more than one registrant per stage.
     */
    inline void run_stage(stage_t stage, const boot_context_t& context) noexcept {
        for (const entry_t* it = __sys_drivers_start; it != __sys_drivers_end; ++it) {
            if (it->stage != stage)
                continue;
            pr_info("init: driver=%s\n", it->name);
            (void)it->fn(&context);
        }
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
