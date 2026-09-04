#pragma once

#include <sys/kernel/printk.hh>
#include <sys/types.hh>

/*
 * Section names and boundary-symbol names, defined once and referenced
 * everywhere else in this file by macro rather than by writing the bare
 * name a second (or third) time -- so a rename touches these four lines
 * and nothing else *in C++*.
 *
 * The one thing a macro can't reach: `src/arch/{arm64,amd64}/kernel.ld`.
 * Those linker scripts are plain text here, not preprocessed, so they
 * cannot expand a C macro -- they must contain these exact literal
 * tokens (`.sys_driver`, `.sys_ops`, `__sys_drivers_start`,
 * `__sys_drivers_end`) written out by hand, and a rename here still
 * means updating both kernel.ld files to match. If that manual sync
 * point becomes a real problem, the fix is to preprocess the linker
 * scripts (kernel.ld.S, run through $(CC) -E) so they can `#include`
 * this same header instead of duplicating the literals -- not attempted
 * here, since the two-file manual sync is still small and explicit.
 */
#define SYS_DRIVER_SECTION_NAME ".sys_driver"
#define SYS_OPS_SECTION_NAME ".sys_ops"
#define SYS_DRIVER_TABLE_BEGIN __sys_drivers_start
#define SYS_DRIVER_TABLE_END __sys_drivers_end

/*
 * Driver-section registration described in
 * docs/architecture/KERNEL_ARCH_PLATFORM_DECOUPLING.md.
 *
 * Every registrant, regardless of which driver it is, lands in one
 * fixed section (SYS_DRIVER_SECTION_NAME for init entries,
 * SYS_OPS_SECTION_NAME for ops-vtable singletons). The linker script
 * KEEP()s those two section names exactly once, permanently -- adding or
 * removing a driver (another PLAT_INIT/ARCH_INIT registration, or
 * another SYS_OPS-tagged ops struct) only ever touches that driver's own
 * .cc file. No kernel.ld edit is needed to add or remove a driver.
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

    /*
     * A device *class*, not a specific board's instance -- borrowed from
     * Linux's driver-core split (struct device / struct device_driver /
     * struct bus_type), scoped down to just the piece that's actually
     * useful with one board and one driver per role: a common, generic
     * descriptor every driver's registration carries, independent of its
     * subsystem-specific ops struct (timer_ops_t, smmu_ops_t, ...). This
     * is what lets kernel-level code (a boot-time device listing, and
     * later the device-ownership capability model tracked as DEV-001 in
     * docs/readiness/PRODUCTION_READINESS_CHECKLIST.md) enumerate "what
     * devices exist" without knowing about any specific subsystem.
     *
     * Linux's dynamic device<->driver matching (of_match_table and
     * friends) is deliberately *not* built here -- there's no second
     * candidate to match against yet. That piece belongs to the deferred
     * DTB-matched platform registry ("Multiple platforms selected via
     * DTB" in the design doc), which is the first place this codebase
     * will have genuine N-candidate matching to justify it.
     *
     * Adding a second *instance* of an existing class (a different UART
     * IP on another board, say) reuses the existing enumerator -- only a
     * genuinely new device *class* needs one added here, same rarity and
     * same non-linker-touching nature as adding a stage_t enumerator.
     */
    enum class device_kind_t : u16 { uart, smmu, timer };

    struct device_t {
        const char* name;
        device_kind_t kind;
    };

    struct boot_context_t {
        uintptr_t firmware_data;
        cpu_id_t cpu_id;
        bool boot_cpu;
    };

    using init_fn_t = error_t (*)(const boot_context_t*) noexcept;

    struct entry_t {
        init_fn_t fn;
        device_t device;
        stage_t stage;
    };

    extern "C" const entry_t SYS_DRIVER_TABLE_BEGIN[];
    extern "C" const entry_t SYS_DRIVER_TABLE_END[];

    /*
     * Iterates the *entire* combined driver table and runs only the
     * entries tagged for `stage`, logging each by its own registered
     * name -- generically, so kernel.hh's call sites never need to name
     * a specific driver in a log message either. Ordering across drivers
     * within the same stage is link-order-dependent; this codebase does
     * not currently rely on more than one registrant per stage.
     */
    inline void run_stage(stage_t stage, const boot_context_t& context) noexcept {
        for (const entry_t* it = SYS_DRIVER_TABLE_BEGIN; it != SYS_DRIVER_TABLE_END; ++it) {
            if (it->stage != stage)
                continue;
            pr_info("init: driver=%s\n", it->device.name);
            (void)it->fn(&context);
        }
    }
} // namespace sys::kernel::init

#define SYS_INIT(stage, kind, fn)                                                                  \
    static const ::sys::kernel::init::entry_t __sys_init_##fn __attribute__((                      \
        used, section(SYS_DRIVER_SECTION_NAME))) = {                                               \
        &fn, {#fn, ::sys::kernel::init::device_kind_t::kind}, ::sys::kernel::init::stage_t::stage}

#define ARCH_INIT(stage, kind, fn) SYS_INIT(stage, kind, fn)
#define PLAT_INIT(stage, kind, fn) SYS_INIT(stage, kind, fn)

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
#define SYS_OPS_DECL __attribute__((section(SYS_OPS_SECTION_NAME)))
#define SYS_OPS __attribute__((used, section(SYS_OPS_SECTION_NAME)))
