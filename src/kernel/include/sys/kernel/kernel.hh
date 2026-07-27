#pragma once

#include <sys/arch/arch.hh>
#include <sys/kernel/address_space.hh>
#include <sys/kernel/capability.hh>
#include <sys/kernel/hypervisor.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/ipc.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduler.hh>
#include <sys/kernel/thread.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/platform.hh>

namespace sys::kernel
{
    inline constexpr const char* name = "zilch";
    inline constexpr u16 version_major = 0U;
    inline constexpr u16 version_minor = 8U;
    inline constexpr u16 version_patch = 0U;

    inline void verify_contracts() noexcept {
        static_assert(arch::v1::compatible(arch::version));
        static_assert(platform::v1::compatible(platform::version));
        static_assert(arch::v1::valid_page_geometry(arch::memory::page_shift,
                                                    arch::memory::virtual_address_bits,
                                                    arch::memory::physical_address_bits));
    }

    [[noreturn]] inline void start_secondary() noexcept {
        platform::console::initialize();
        pr_info("smp: secondary entered cpu=%u el=%u\n",
                static_cast<unsigned int>(arch::cpu::current_id()),
                static_cast<unsigned int>(arch::exception::current_el()));
        arch::exception::initialize_current_el();
        if (!arch::hypervisor::initialize_cpu()) {
            pr_err("hypervisor self-test failed cpu=%u\n",
                   static_cast<unsigned int>(arch::cpu::current_id()));
            arch::cpu::halt();
        }
        const error_t interrupt_result = platform::interrupt::initialize_cpu();
        if (interrupt_result != error_t::success) {
            arch::cpu::halt();
        }
        platform::timer::initialize();
        arch::memory::initialize_cpu();
        scheduler::initialize_cpu();
        arch::smp::mark_online();
        arch::irq::enable();
        if constexpr (arch::space::user_available) {
            thread::wait_until_ready();
            thread::enter_first_user_thread();
        }
        arch::cpu::halt();
    }

    [[noreturn]] inline void start() noexcept {
        verify_contracts();

        platform::console::initialize();
        arch::cpu::initialize_boot_cpu();
        arch::exception::initialize_current_el();
        if (!arch::hypervisor::initialize_cpu()) {
            pr_err("hypervisor self-test failed cpu=%u\n",
                   static_cast<unsigned int>(arch::cpu::current_id()));
            arch::cpu::halt();
        }
        arch::memory::initialize();
        scheduler::initialize();
        scheduler::initialize_cpu();

        pr_info("%s L4 microkernel %u.%u.%u\n", name, static_cast<unsigned int>(version_major),
                static_cast<unsigned int>(version_minor), static_cast<unsigned int>(version_patch));
        pr_info("arch=%s platform=%s word_bits=%u el=%u\n", arch::name, platform::name,
                static_cast<unsigned int>(word_bits),
                static_cast<unsigned int>(arch::exception::current_el()));

        pr_info("gic: initializing distributor and boot CPU interface\n");
        const error_t interrupt_result = platform::interrupt::initialize();
        if (interrupt_result != error_t::success) {
            pr_err("interrupt initialization failed=%d\n", static_cast<int>(interrupt_result));
            arch::cpu::halt();
        }

        pr_info("gic: initialized\n");
        pr_info("timer: initializing\n");
        platform::timer::initialize();
        pr_info("timer: initialized\n");
        arch::smp::mark_online();
        pr_info("smp: boot CPU online\n");

        const error_t smp_result = arch::smp::boot_secondary_cpus();
        if (smp_result != error_t::success) {
            pr_err("secondary CPU startup failed=%d\n", static_cast<int>(smp_result));
            arch::cpu::halt();
        }

        const u32 expected = platform::firmware::boot_info.cpu_count;
        const bool online = arch::smp::wait_until_online(expected, 1000000U);
        pr_info("smp cpus=%u/%u status=%s\n", static_cast<unsigned int>(arch::smp::online_count()),
                static_cast<unsigned int>(expected), online ? "online" : "timeout");
        if constexpr (arch::space::user_available) {
            const error_t user_result = thread::initialize_user_threads();
            if (user_result == error_t::success) {
                pr_info("memory: base=%llx pages=%u free=%u page_size=%llu\n",
                        static_cast<unsigned long long>(memory::managed_base),
                        static_cast<unsigned int>(memory::managed_pages),
                        static_cast<unsigned int>(memory::free_pages),
                        static_cast<unsigned long long>(memory::page_size));
            }
            if (user_result != error_t::success) {
                pr_err("user object initialization failed=%d\n", static_cast<int>(user_result));
                arch::cpu::halt();
            }
#if CONFIG_SELFTEST
            const error_t bootstrap_result = thread::validate_bootstrap_objects();
            if (bootstrap_result != error_t::success) {
                pr_err("kernel bootstrap self-test failed=%d\n",
                       static_cast<int>(bootstrap_result));
                arch::cpu::halt();
            }
            pr_info("kernel certification harness=enabled\n");
#else
            pr_info("kernel product build selftests=disabled\n");
#endif
            pr_info("root bootstrap: task=0 bootinfo=v%u caps=%u pager_endpoint=%u mode=%s\n",
                    static_cast<unsigned int>(boot::root_bootinfo.version),
                    static_cast<unsigned int>(boot::root_bootinfo.capability_count),
                    static_cast<unsigned int>(boot::root_bootinfo.root_fault_endpoint),
                    CONFIG_ROOT_ONLY_BOOT ? "root-only" : "compatibility-fuzz");
            pr_info("user authority: tasks=%u cspaces=%u endpoints=%u "
                    "object-table=generation-checked\n",
                    static_cast<unsigned int>(thread::user_thread_count),
                    static_cast<unsigned int>(thread::user_thread_count),
                    static_cast<unsigned int>(ipc::endpoint_count));
            if constexpr (CONFIG_ROOT_ONLY_BOOT) {
                pr_info("root-only boot: initial_tasks=1 initial_threads=1 image=init.elf\n");
            } else {
                pr_info("user smp: address-spaces=%u threads=%u cpus=%u "
                        "ipc=capability-call/reply_receive fault-ipc=enabled fuzz=deterministic\n",
                        static_cast<unsigned int>(thread::active_user_thread_count),
                        static_cast<unsigned int>(thread::active_user_thread_count),
                        static_cast<unsigned int>(expected));
            }
        }
        if constexpr (arch::hypervisor::active) {
            pr_info("exceptions=EL1 hypervisor=EL2 gic=GICv3 timer=virtual@%uHz\n",
                    static_cast<unsigned int>(platform::timer::ticks_per_second));
            pr_info("hypervisor cpus=%u/%u hvc=verified\n",
                    static_cast<unsigned int>(arch::hypervisor::verified_count()),
                    static_cast<unsigned int>(expected));
        } else {
            pr_info("exceptions=kernel hypervisor=inactive timer=%uHz\n",
                    static_cast<unsigned int>(platform::timer::ticks_per_second));
        }

        arch::irq::enable();
        pr_info("status=interrupts-enabled\n");

        platform::interrupt::send_ipi_all_others(platform::interrupt::reschedule_ipi);

        bool ipi_online = false;
        for (u64 spins = 1000000U; spins != 0U; --spins) {
            ipi_online = true;
            for (u32 cpu_id = 1U; cpu_id < expected; ++cpu_id) {
                if (arch::smp::reschedule_ipis(cpu_id) == 0U) {
                    ipi_online = false;
                    break;
                }
            }
            if (ipi_online) {
                break;
            }
            arch::cpu::relax();
        }

        pr_info("ipi reschedule=%s targets=%u\n", ipi_online ? "verified" : "timeout",
                static_cast<unsigned int>(expected - 1U));

        bool timers_online = false;
        for (u64 spins = 1000000U; spins != 0U; --spins) {
            timers_online = true;
            for (u32 cpu_id = 0U; cpu_id < expected; ++cpu_id) {
                if (platform::timer::ticks(cpu_id) == 0U) {
                    timers_online = false;
                    break;
                }
            }
            if (timers_online) {
                break;
            }
            arch::cpu::relax();
        }

        pr_info("timer per-cpu=%s cpus=%u\n", timers_online ? "verified" : "timeout",
                static_cast<unsigned int>(expected));

        platform::interrupt::send_ipi_all_others(platform::interrupt::tlb_shootdown_ipi);
        bool tlb_online = false;
        for (u64 spins = 1000000U; spins != 0U; --spins) {
            tlb_online = true;
            for (u32 cpu_id = 1U; cpu_id < expected; ++cpu_id) {
                if (arch::smp::tlb_shootdown_ipis(cpu_id) == 0U) {
                    tlb_online = false;
                    break;
                }
            }
            if (tlb_online)
                break;
            arch::cpu::relax();
        }
        pr_info("tlb shootdown=%s targets=%u asids=%u\n", tlb_online ? "verified" : "timeout",
                static_cast<unsigned int>(expected - 1U),
                static_cast<unsigned int>(thread::user_thread_count));

        if constexpr (arch::space::user_available) {
            thread::log_pinning_table(CONFIG_ROOT_ONLY_BOOT ? 1U : expected);
            thread::launch_user_scheduler();
            if constexpr (CONFIG_ROOT_ONLY_BOOT) {
                pr_info("root-only boot: cpu=0 entering init.elf entry=%llx\n",
                        static_cast<unsigned long long>(
                            arch::space::entry(thread::user_threads[0].address_space.native)));
            } else {
                pr_info("user smp: cpu=0 entering server thread=0 seed=%llx\n",
                        static_cast<unsigned long long>(thread::user_threads[0].fuzz_seed));
            }
            thread::enter_first_user_thread();
        }
        arch::cpu::halt();
    }
} // namespace sys::kernel
