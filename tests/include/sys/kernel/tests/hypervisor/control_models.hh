#pragma once

#include <sys/kernel/hypervisor.hh>

#if !CONFIG_HYPERVISOR_SELFTEST
#error "hypervisor control models require CONFIG_HYPERVISOR_SELFTEST"
#endif

namespace sys::kernel::hypervisor::test
{
    extern "C" const u8 sys_arm64_guest_image_start[];
    extern "C" const u8 sys_arm64_guest_image_end[];

    alignas(4096) inline u64 stage2_l1[512]{};
    alignas(4096) inline u64 stage2_l2[512]{};
    alignas(4096) inline u64 stage2_l3[512]{};
    alignas(4096) inline u8 guest_ram[guest_ram_size]{};

    inline void clear_words(u64* values, u32 count) noexcept {
        for (u32 index = 0U; index < count; ++index)
            values[index] = 0U;
    }

    [[nodiscard]] inline error_t prepare_bootstrap_guest() noexcept {
        const u64 image_size =
            static_cast<u64>(sys_arm64_guest_image_end - sys_arm64_guest_image_start);
        if (image_size == 0U || image_size > guest_ram_size)
            return error_t::invalid_argument;
        for (u64 index = 0U; index < guest_ram_size; ++index)
            guest_ram[index] = 0U;
        for (u64 index = 0U; index < image_size; ++index)
            guest_ram[index] = sys_arm64_guest_image_start[index];

        clear_words(stage2_l1, 512U);
        clear_words(stage2_l2, 512U);
        clear_words(stage2_l3, 512U);
        const paddr_t l2 = reinterpret_cast<paddr_t>(&stage2_l2[0]);
        const paddr_t l3 = reinterpret_cast<paddr_t>(&stage2_l3[0]);
        const paddr_t ram = reinterpret_cast<paddr_t>(&guest_ram[0]);
        stage2_l1[0] = (l2 & 0x0000fffffffff000ULL) | 0x3ULL;
        stage2_l2[0] = (l3 & 0x0000fffffffff000ULL) | 0x3ULL;
        constexpr u64 normal_memory = 0xfULL << 2U;
        constexpr u64 inner_shareable = 0x3ULL << 8U;
        constexpr u64 access_flag = 1ULL << 10U;
        constexpr u64 stage2_read_only = 1ULL << 6U;
        constexpr u64 stage2_read_write = 3ULL << 6U;
        constexpr u64 stage2_execute_never = 1ULL << 54U;
        constexpr u32 executable_pages = 4U;
        for (u32 page = 0U; page < guest_ram_pages; ++page) {
            const paddr_t physical = ram + static_cast<paddr_t>(page) * page_size;
            const bool executable = page < executable_pages;
            stage2_l3[page] =
                (physical & 0x0000fffffffff000ULL) | 0x3ULL | normal_memory | inner_shareable |
                access_flag |
                (executable ? stage2_read_only : stage2_read_write | stage2_execute_never);
        }
        bootstrap_vm.stage_2_root = reinterpret_cast<paddr_t>(&stage2_l1[0]);
        const error_t reset_result = reset(bootstrap_vm);
        if (reset_result != error_t::success)
            return reset_result;
        const u64 executable_size = static_cast<u64>(executable_pages) * page_size;
        const error_t code_map_result =
            stage2_map(bootstrap_vm, 0U, ram, executable_size,
                       static_cast<u32>(stage2_permission::read) |
                           static_cast<u32>(stage2_permission::execute));
        if (code_map_result != error_t::success)
            return code_map_result;
        const error_t data_map_result = stage2_map(
            bootstrap_vm, executable_size, ram + executable_size, guest_ram_size - executable_size,
            static_cast<u32>(stage2_permission::read) | static_cast<u32>(stage2_permission::write));
        if (data_map_result != error_t::success)
            return data_map_result;
        for (u64 offset = 0U; offset < image_size; offset += 64U) {
            const paddr_t address = ram + offset;
            __asm__ volatile("dc cvau, %0" ::"r"(address) : "memory");
        }
        __asm__ volatile("dsb ish; ic iallu; dsb ish; isb" ::: "memory");
        // Enter guest EL1h with D/A/I/F masked until the guest installs VBAR_EL1.
        // A pending host PPI must not reach the guest reset trampoline.
        constexpr u64 guest_reset_pstate = 0x3c5U;
        const error_t configure_result =
            configure_vcpu(bootstrap_vcpu, 0U, guest_reset_pstate, 0xf000U);
        if (configure_result != error_t::success)
            return configure_result;
        // Guest starts at EL1h with its stage-1 MMU and caches disabled.
        // SCTLR_EL1 RES1 bits: 29, 28, 23, 22, 20, and 11.
        bootstrap_vcpu.context.sctlr_el1 = 0x30d00800ULL;
        bootstrap_vcpu.context.tcr_el1 = 0U;
        bootstrap_vcpu.context.ttbr0_el1 = 0U;
        bootstrap_vcpu.context.ttbr1_el1 = 0U;
        bootstrap_vcpu.context.mair_el1 = 0U;
        bootstrap_vcpu.context.vbar_el1 = 0U;
        bootstrap_vcpu.context.sp_el1 = 0xf000U;
        bootstrap_vcpu.context.elr_el1 = 0U;
        bootstrap_vcpu.context.spsr_el1 = guest_reset_pstate;
        bootstrap_vcpu.context.cntv_ctl_el0 = 0U;
        bootstrap_vcpu.context.cntv_cval_el0 = 0U;
        bootstrap_vm.counter_offset = 0x1000U;
        return error_t::success;
    }

    [[nodiscard]] inline error_t run_bootstrap_guest() noexcept {
        exit_record exit{};
        const error_t preparation = prepare_bootstrap_guest();
        if (preparation != error_t::success) {
            diagnose(bootstrap_vm, 30U, preparation);
            return preparation;
        }
        pr_info("[HV-D] guest-entry vmid=%u root=%llx ram=%llx size=%llu\n", bootstrap_vm.vmid,
                static_cast<unsigned long long>(bootstrap_vm.stage_2_root),
                static_cast<unsigned long long>(reinterpret_cast<paddr_t>(&guest_ram[0])),
                static_cast<unsigned long long>(guest_ram_size));
        error_t result = run(bootstrap_vcpu, exit);
        if (result != error_t::success)
            return result;
        if (exit.reason != abi::v1::vm_exit_reason::wait) {
            diagnose(bootstrap_vm, 31U, error_t::invalid_argument, exit.guest_pc, exit.syndrome);
            return error_t::invalid_argument;
        }
        if ((bootstrap_vcpu.context.cntv_ctl_el0 & 1U) == 0U ||
            bootstrap_vcpu.context.cntv_cval_el0 == 0U || !bootstrap_vcpu.timer.armed ||
            bootstrap_vcpu.timer.deadline != bootstrap_vcpu.context.cntv_cval_el0) {
            diagnose(bootstrap_vm, 33U, error_t::invalid_argument,
                     bootstrap_vcpu.context.cntv_ctl_el0, bootstrap_vcpu.context.cntv_cval_el0);
            return error_t::invalid_argument;
        }
        pr_info("[HV-G] guest-mmu-vectors result=PASS pc=%llx\n",
                static_cast<unsigned long long>(exit.guest_pc));
        pr_info("[HV-GT] virtual-timer-state result=PASS ctl=%llx cval=%llx offset=%llx\n",
                static_cast<unsigned long long>(bootstrap_vcpu.context.cntv_ctl_el0),
                static_cast<unsigned long long>(bootstrap_vcpu.context.cntv_cval_el0),
                static_cast<unsigned long long>(bootstrap_vm.counter_offset));
        const error_t inject_result = inject_irq(bootstrap_vcpu, 27U);
        if (inject_result != error_t::success)
            return inject_result;
        exit = {};
        result = run(bootstrap_vcpu, exit);
        if (result != error_t::success)
            return result;
        constexpr u64 required_reports = 0x7ULL;
        if (exit.reason != abi::v1::vm_exit_reason::shutdown ||
            (exit.qualification & required_reports) != required_reports) {
            diagnose(bootstrap_vm, 32U, error_t::invalid_argument, exit.guest_pc,
                     exit.qualification);
            return error_t::invalid_argument;
        }
        pr_info("[HV-H] virtual-timer-irq result=PASS irq=27 reports=%llx\n",
                static_cast<unsigned long long>(exit.qualification));
        pr_info("[HV-I] guest-el0-svc result=PASS pc=%llx\n",
                static_cast<unsigned long long>(exit.guest_pc));
        return error_t::success;
    }

    inline volatile u64 operations{};
    inline volatile u64 failures_total{};
    inline virtual_interrupt_state model_basic_vic{};
    inline virtual_cpu_t model_basic_lifecycle_vcpu{};
    inline virtual_machine_t model_basic_isolation_a{};
    inline virtual_machine_t model_basic_isolation_b{};
    inline virtual_cpu_t model_basic_teardown_vcpus[2]{};
    inline virtual_machine_t model_multivcpu_vm_a{};
    inline virtual_machine_t model_multivcpu_vm_b{};
    inline virtual_cpu_t model_multivcpu_vcpus_a[maximum_vcpus_per_vm]{};
    inline virtual_cpu_t model_multivcpu_vcpus_b[2]{};
    inline virtual_cpu_t real_smp_vcpus[maximum_vcpus_per_vm]{};
    inline virtual_cpu_t* real_smp_job_vcpus[maximum_vcpus_per_vm]{};
    inline exit_record real_smp_exits[maximum_vcpus_per_vm]{};
    inline error_t real_smp_status[maximum_vcpus_per_vm]{};
    inline u32 real_smp_vcpu_for_cpu[maximum_vcpus_per_vm]{};
    inline volatile u32 real_smp_claimed_mask{};
    inline volatile u32 real_smp_done_mask{};
    inline volatile bool real_smp_active{};

    inline void service_real_smp_job(cpu_id_t cpu) noexcept {
        if (!__atomic_load_n(&real_smp_active, __ATOMIC_ACQUIRE) || cpu >= maximum_vcpus_per_vm)
            return;
        const u32 bit = 1U << cpu;
        if ((__atomic_fetch_or(&real_smp_claimed_mask, bit, __ATOMIC_ACQ_REL) & bit) != 0U)
            return;
        const u32 vcpu_index = real_smp_vcpu_for_cpu[cpu];
        real_smp_exits[vcpu_index] = {};
        auto* vcpu = real_smp_job_vcpus[vcpu_index];
        real_smp_status[vcpu_index] =
            vcpu == nullptr ? error_t::success : run(*vcpu, real_smp_exits[vcpu_index]);
        __atomic_fetch_or(&real_smp_done_mask, bit, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool dispatch_real_smp_phase(bool migrate) noexcept {
        __atomic_store_n(&real_smp_claimed_mask, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&real_smp_done_mask, 0U, __ATOMIC_RELEASE);
        for (u32 cpu = 0U; cpu < maximum_vcpus_per_vm; ++cpu)
            real_smp_vcpu_for_cpu[cpu] =
                migrate ? (cpu + maximum_vcpus_per_vm - 1U) % maximum_vcpus_per_vm : cpu;
        __atomic_store_n(&real_smp_active, true, __ATOMIC_RELEASE);
        for (cpu_id_t cpu = 1U; cpu < maximum_vcpus_per_vm; ++cpu)
            platform::interrupt::send_ipi(cpu, platform::interrupt::reschedule_ipi);
        service_real_smp_job(0U);
        constexpr u32 all_cpus = (1U << maximum_vcpus_per_vm) - 1U;
        u32 spins = 0U;
        while (__atomic_load_n(&real_smp_done_mask, __ATOMIC_ACQUIRE) != all_cpus &&
               spins++ < 1000000U)
            arch::cpu::relax();
        __atomic_store_n(&real_smp_active, false, __ATOMIC_RELEASE);
        return __atomic_load_n(&real_smp_done_mask, __ATOMIC_ACQUIRE) == all_cpus;
    }

    [[nodiscard]] inline bool real_smp_acceptance() noexcept {
        if (prepare_bootstrap_guest() != error_t::success)
            return false;
        *reinterpret_cast<volatile u32*>(&guest_ram[0x5000]) = 0U;
        *reinterpret_cast<volatile u32*>(&guest_ram[0x5008]) = 0U;
        constexpr u64 smp_magic = 0x5054534dULL;
        constexpr u64 guest_reset_pstate = 0x3c5U;
        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
            auto& vcpu = real_smp_vcpus[index];
            scrub_bytes(&vcpu, sizeof(vcpu));
            vcpu.id = 0x100U + index;
            vcpu.logical_id = index;
            vcpu.virtual_machine = object::reference(bootstrap_vm.object);
            vcpu.state = vm_state::configured;
            vcpu.lifecycle = vcpu_state::configured;
            if (configure_vcpu(vcpu, 0U, guest_reset_pstate,
                               0xf000U - static_cast<u64>(index) * 0x1000U) != error_t::success)
                return false;
            vcpu.context.x[19] = smp_magic;
            vcpu.context.x[20] = index;
            vcpu.context.x[22] = maximum_vcpus_per_vm;
            vcpu.context.sctlr_el1 = arch::hypervisor::sanitize_guest_sctlr_el1(0U);
            real_smp_job_vcpus[index] = &vcpu;
        }
        if (!dispatch_real_smp_phase(false)) {
            pr_err("[HV-REAL-SMP] phase=entry done=%x result=FAIL\n",
                   __atomic_load_n(&real_smp_done_mask, __ATOMIC_ACQUIRE));
            return false;
        }
        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
            if (real_smp_status[index] != error_t::success ||
                real_smp_exits[index].reason != abi::v1::vm_exit_reason::wait) {
                pr_err(
                    "[HV-REAL-SMP] phase=entry vcpu=%u status=%d reason=%u pc=%llx result=FAIL\n",
                    index, static_cast<int>(real_smp_status[index]),
                    static_cast<unsigned int>(real_smp_exits[index].reason),
                    static_cast<unsigned long long>(real_smp_exits[index].guest_pc));
                return false;
            }
            if (inject_irq(real_smp_vcpus[index], static_cast<u16>(index)) != error_t::success)
                return false;
        }
        if (!dispatch_real_smp_phase(true)) {
            pr_err("[HV-REAL-SMP] phase=migrate done=%x result=FAIL\n",
                   __atomic_load_n(&real_smp_done_mask, __ATOMIC_ACQUIRE));
            return false;
        }
        bool interrupt_boundary_exit = true;
        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index)
            interrupt_boundary_exit = interrupt_boundary_exit &&
                                      real_smp_exits[index].reason == abi::v1::vm_exit_reason::wait;
        if (interrupt_boundary_exit && !dispatch_real_smp_phase(false)) {
            pr_err("[HV-REAL-SMP] phase=interrupt-reentry done=%x result=FAIL\n",
                   __atomic_load_n(&real_smp_done_mask, __ATOMIC_ACQUIRE));
            return false;
        }
        u32 migrated = 0U;
        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
            const u64 reports = 0x100ULL << index;
            if (real_smp_status[index] != error_t::success ||
                real_smp_exits[index].reason != abi::v1::vm_exit_reason::shutdown ||
                (real_smp_exits[index].qualification & reports) != reports) {
                pr_err("[HV-REAL-SMP] phase=migrate vcpu=%u status=%d reason=%u reports=%llx "
                       "required=%llx pc=%llx result=FAIL\n",
                       index, static_cast<int>(real_smp_status[index]),
                       static_cast<unsigned int>(real_smp_exits[index].reason),
                       static_cast<unsigned long long>(real_smp_exits[index].qualification),
                       static_cast<unsigned long long>(reports),
                       static_cast<unsigned long long>(real_smp_exits[index].guest_pc));
                return false;
            }
            if (real_smp_vcpus[index].migration_count != 0U)
                ++migrated;
        }
        const u32 barrier = *reinterpret_cast<volatile u32*>(&guest_ram[0x5000]);
        const u32 irqs = *reinterpret_cast<volatile u32*>(&guest_ram[0x5008]);
        if (barrier != maximum_vcpus_per_vm || irqs != maximum_vcpus_per_vm ||
            migrated != maximum_vcpus_per_vm) {
            pr_err("[HV-REAL-SMP] barrier=%u irqs=%u migrations=%u result=FAIL\n", barrier, irqs,
                   migrated);
            return false;
        }
        pr_info("[HV-REAL-SMP] physical-cpus=4 guest-streams=4 barrier=%u irqs=%u "
                "migrations=%u result=PASS\n",
                barrier, irqs, migrated);
        pr_info("[TEST] name=hypervisor_real_smp_execution result=PASS\n");
        return true;
    }

    alignas(page_size) inline u8 real_multivm_ram[2][guest_ram_size]{};

    [[nodiscard]] inline bool prepare_real_vm(virtual_machine_t*& vm, virtual_cpu_t*& first,
                                              virtual_cpu_t*& second, u32 tag) noexcept {
        if (create_vm(0x2000U * (tag + 1U), vm) != error_t::success)
            return false;
        const u64 image_size =
            static_cast<u64>(sys_arm64_guest_image_end - sys_arm64_guest_image_start);
        for (u64 offset = 0U; offset < guest_ram_size; ++offset)
            real_multivm_ram[tag][offset] = 0U;
        for (u64 offset = 0U; offset < image_size; ++offset)
            real_multivm_ram[tag][offset] = sys_arm64_guest_image_start[offset];
        const paddr_t ram = reinterpret_cast<paddr_t>(&real_multivm_ram[tag][0]);
        if (stage2_map(*vm, 0U, ram, 4U * page_size,
                       static_cast<u32>(stage2_permission::read) |
                           static_cast<u32>(stage2_permission::execute)) != error_t::success ||
            stage2_map(*vm, 4U * page_size, ram + 4U * page_size, guest_ram_size - 4U * page_size,
                       static_cast<u32>(stage2_permission::read) |
                           static_cast<u32>(stage2_permission::write)) != error_t::success)
            return false;
        if (create_vcpu(*vm, 0U, first) != error_t::success ||
            create_vcpu(*vm, 1U, second) != error_t::success)
            return false;
        virtual_cpu_t* pair[2]{first, second};
        constexpr u64 smp_magic = 0x5054534dULL;
        for (u32 index = 0U; index < 2U; ++index) {
            if (configure_vcpu(*pair[index], 0U, 0x3c5U,
                               0xf000U - static_cast<u64>(index) * 0x1000U) != error_t::success)
                return false;
            pair[index]->context.x[19] = smp_magic;
            pair[index]->context.x[20] = index;
            pair[index]->context.x[22] = 2U;
        }
        for (u64 offset = 0U; offset < image_size; offset += 64U) {
            const paddr_t address = ram + offset;
            __asm__ volatile("dc cvau, %0" ::"r"(address) : "memory");
        }
        __asm__ volatile("dsb ish; ic iallu; dsb ish; isb" ::: "memory");
        return true;
    }

    inline void cleanup_real_vm(virtual_machine_t* vm, virtual_cpu_t* first,
                                virtual_cpu_t* second) noexcept {
        if (first != nullptr)
            (void)destroy_vcpu(*first);
        if (second != nullptr)
            (void)destroy_vcpu(*second);
        if (vm != nullptr) {
            (void)stage2_unmap(*vm, 0U);
            (void)stage2_unmap(*vm, 4U * page_size);
            (void)destroy_vm(*vm);
        }
    }

    [[nodiscard]] inline bool real_multivm_acceptance() noexcept {
        virtual_machine_t* vm_a = nullptr;
        virtual_machine_t* vm_b = nullptr;
        virtual_cpu_t* a0 = nullptr;
        virtual_cpu_t* a1 = nullptr;
        virtual_cpu_t* b0 = nullptr;
        virtual_cpu_t* b1 = nullptr;
        if (!prepare_real_vm(vm_a, a0, a1, 0U) || !prepare_real_vm(vm_b, b0, b1, 1U)) {
            cleanup_real_vm(vm_a, a0, a1);
            cleanup_real_vm(vm_b, b0, b1);
            return false;
        }
        real_smp_job_vcpus[0] = a0;
        real_smp_job_vcpus[1] = a1;
        real_smp_job_vcpus[2] = b0;
        real_smp_job_vcpus[3] = b1;
        if (!dispatch_real_smp_phase(false)) {
            cleanup_real_vm(vm_a, a0, a1);
            cleanup_real_vm(vm_b, b0, b1);
            return false;
        }
        bool passed = true;
        for (u32 index = 0U; index < 4U; ++index)
            passed = passed && real_smp_status[index] == error_t::success &&
                     real_smp_exits[index].reason == abi::v1::vm_exit_reason::wait;
        const u32 barrier_a = *reinterpret_cast<volatile u32*>(&real_multivm_ram[0][0x5000]);
        const u32 barrier_b = *reinterpret_cast<volatile u32*>(&real_multivm_ram[1][0x5000]);
        passed = passed && barrier_a == 2U && barrier_b == 2U && mappings_isolated(*vm_a, *vm_b);

        /* Crash one VM through an unmapped IPA and prove the peer remains runnable. */
        a0->context.pc = guest_ipa_limit - page_size;
        a0->state = vm_state::runnable;
        a0->lifecycle = vcpu_state::runnable;
        exit_record crash{};
        const error_t crash_status = run(*a0, crash);
        passed = passed && crash_status == error_t::success &&
                 (crash.reason == abi::v1::vm_exit_reason::stage2_fault ||
                  crash.reason == abi::v1::vm_exit_reason::unexpected) &&
                 b0->state == vm_state::runnable && b1->state == vm_state::runnable &&
                 vm_b->state != vm_state::faulted;
        a0->state = vm_state::stopped;
        a0->lifecycle = vcpu_state::stopped;
        cleanup_real_vm(vm_a, a0, a1);
        cleanup_real_vm(vm_b, b0, b1);
        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index)
            real_smp_job_vcpus[index] = &real_smp_vcpus[index];
        if (passed) {
            pr_info("[HV-REAL-MULTIVM] vms=2 vcpus=4 barriers=2,2 crash-isolation=PASS "
                    "result=PASS\n");
            pr_info("[TEST] name=hypervisor_real_multivm_isolation result=PASS\n");
        } else {
            pr_err("[HV-REAL-MULTIVM] barrier-a=%u barrier-b=%u crash-reason=%u result=FAIL\n",
                   barrier_a, barrier_b, static_cast<unsigned int>(crash.reason));
        }
        return passed;
    }

    enum class guest_cpu_boot_state : u8 { off, starting, online, parked, halted };

    struct model_context_cpu_state {
        guest_cpu_boot_state boot_state{guest_cpu_boot_state::off};
        u64 entry_pc{};
        u64 stack_pointer{};
        u64 boot_cookie{};
        u64 completed_steps{};
        u64 received_ipis{};
        u64 timer_events{};
        u64 last_resume_pc{};
    };

    struct model_context_counters {
        u32 booted{};
        u32 reentries{};
        u32 ipis{};
        u32 timer_events{};
        u32 migrations{};
        u32 barriers{};
        u32 teardown_busy{};
    };

    inline model_context_cpu_state model_context_cpu_states[maximum_vcpus_per_vm]{};
    inline model_context_counters model_context_stats{};

    struct model_multivcpu_counters {
        u32 online{};
        u32 barriers{};
        u32 ipis{};
        u32 timer_irqs{};
        u32 quanta{};
        u32 migrations{};
        u32 teardown_busy{};
    };

    inline model_multivcpu_counters model_multivcpu_stats{};

    inline void initialize_model_multivcpu_vcpu(virtual_cpu_t& vcpu, u32 logical_id,
                                                cpu_id_t host_cpu) noexcept {
        vcpu.state = vm_state::runnable;
        vcpu.lifecycle = vcpu_state::runnable;
        vcpu.logical_id = logical_id;
        vcpu.host_cpu = host_cpu;
        vcpu.previous_host_cpu = host_cpu;
        vcpu.migration_count = 0U;
        vcpu.executed_quanta = 0U;
        vcpu.virtual_ipi_count = 0U;
        vcpu.timer.deadline = 0U;
        vcpu.timer.armed = false;
        vcpu.running = false;
        vcpu.virtual_irq_pending = false;
        vcpu.interrupt_state.reset();
    }

    [[nodiscard]] inline error_t arm_virtual_timer(virtual_cpu_t& vcpu, u64 deadline) noexcept {
        if (vcpu.lifecycle == vcpu_state::inactive || vcpu.lifecycle == vcpu_state::stopped)
            return error_t::invalid_argument;
        vcpu.timer.deadline = deadline;
        vcpu.timer.armed = true;
        return error_t::success;
    }

    [[nodiscard]] inline error_t send_virtual_ipi(virtual_cpu_t& target, u16 irq = 1U) noexcept {
        const error_t result = target.interrupt_state.inject(irq);
        if (result != error_t::success)
            return result;
        ++target.virtual_ipi_count;
        ++model_multivcpu_stats.ipis;
        return error_t::success;
    }

    [[nodiscard]] inline error_t
    schedule_model_multivcpu_quantum(virtual_cpu_t& vcpu, cpu_id_t host_cpu, u64 tick) noexcept {
        if (vcpu.lifecycle != vcpu_state::runnable)
            return error_t::invalid_argument;
        vcpu.lifecycle = vcpu_state::running;
        vcpu.running = true;
        vcpu.previous_host_cpu = vcpu.host_cpu;
        vcpu.host_cpu = host_cpu;
        if (vcpu.executed_quanta != 0U && vcpu.previous_host_cpu != vcpu.host_cpu) {
            ++vcpu.migration_count;
            ++model_multivcpu_stats.migrations;
        }
        if (vcpu.timer.armed && tick >= vcpu.timer.deadline) {
            const error_t timer_result = vcpu.interrupt_state.inject(27U);
            if (timer_result != error_t::success)
                return timer_result;
            vcpu.timer.armed = false;
            ++model_multivcpu_stats.timer_irqs;
        }
        ++vcpu.executed_quanta;
        ++model_multivcpu_stats.quanta;
        vcpu.running = false;
        vcpu.lifecycle = vcpu_state::runnable;
        return error_t::success;
    }

    [[nodiscard]] inline bool model_multivcpu_acceptance() noexcept {
        model_multivcpu_stats = {};
        virtual_machine_t& vm_a = model_multivcpu_vm_a;
        virtual_machine_t& vm_b = model_multivcpu_vm_b;
        if (allocate_vmid(vm_a.vmid) != error_t::success)
            return false;
        if (allocate_vmid(vm_b.vmid) != error_t::success) {
            (void)release_vmid(vm_a.vmid);
            return false;
        }
        vm_a.state = vm_state::runnable;
        vm_b.state = vm_state::runnable;
        vm_a.stage_2_root = 0x51000000U;
        vm_b.stage_2_root = 0x52000000U;
        vm_a.mappings[0] = {0U, 0x61000000U, page_size,
                            static_cast<u32>(stage2_permission::read) |
                                static_cast<u32>(stage2_permission::write),
                            true};
        vm_b.mappings[0] = {0U, 0x62000000U, page_size,
                            static_cast<u32>(stage2_permission::read) |
                                static_cast<u32>(stage2_permission::write),
                            true};
        vm_a.mapping_count = 1U;
        vm_b.mapping_count = 1U;

        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
            initialize_model_multivcpu_vcpu(model_multivcpu_vcpus_a[index], index,
                                            static_cast<cpu_id_t>(index));
            ++model_multivcpu_stats.online;
            if (arm_virtual_timer(model_multivcpu_vcpus_a[index], 8U + index) != error_t::success)
                return false;
        }
        for (u32 index = 0U; index < 2U; ++index) {
            initialize_model_multivcpu_vcpu(model_multivcpu_vcpus_b[index], index,
                                            static_cast<cpu_id_t>(index + 2U));
            if (arm_virtual_timer(model_multivcpu_vcpus_b[index], 10U + index) != error_t::success)
                return false;
        }
        if (model_multivcpu_stats.online != maximum_vcpus_per_vm)
            return false;
        ++model_multivcpu_stats.barriers;

        for (u32 index = 1U; index < maximum_vcpus_per_vm; ++index) {
            if (send_virtual_ipi(model_multivcpu_vcpus_a[index]) != error_t::success)
                return false;
        }

        for (u64 tick = 0U; tick < 64U; ++tick) {
            for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
                const cpu_id_t cpu = static_cast<cpu_id_t>((index + tick) % maximum_vcpus_per_vm);
                if (schedule_model_multivcpu_quantum(model_multivcpu_vcpus_a[index], cpu, tick) !=
                    error_t::success)
                    return false;
            }
            for (u32 index = 0U; index < 2U; ++index) {
                const cpu_id_t cpu =
                    static_cast<cpu_id_t>((index + tick + 1U) % maximum_vcpus_per_vm);
                if (schedule_model_multivcpu_quantum(model_multivcpu_vcpus_b[index], cpu, tick) !=
                    error_t::success)
                    return false;
            }
        }

        for (u32 index = 1U; index < maximum_vcpus_per_vm; ++index) {
            u16 irq = 0U;
            if (model_multivcpu_vcpus_a[index].interrupt_state.acknowledge(irq) !=
                    error_t::success ||
                irq != 1U ||
                model_multivcpu_vcpus_a[index].interrupt_state.deactivate(irq) != error_t::success)
                return false;
        }
        if (!mappings_isolated(vm_a, vm_b))
            return false;
        if (model_multivcpu_stats.timer_irqs != maximum_vcpus_per_vm + 2U)
            return false;
        if (model_multivcpu_stats.migrations == 0U || model_multivcpu_stats.quanta != 384U)
            return false;

        model_multivcpu_vcpus_a[0].running = true;
        if (teardown_vm(vm_a, model_multivcpu_vcpus_a, maximum_vcpus_per_vm) != error_t::busy)
            return false;
        ++model_multivcpu_stats.teardown_busy;
        model_multivcpu_vcpus_a[0].running = false;
        const u16 old_a = vm_a.vmid;
        if (teardown_vm(vm_a, model_multivcpu_vcpus_a, maximum_vcpus_per_vm) != error_t::success)
            return false;
        if (teardown_vm(vm_b, model_multivcpu_vcpus_b, 2U) != error_t::success)
            return false;
        u16 reused = 0U;
        if (allocate_vmid(reused) != error_t::success || reused != old_a)
            return false;
        if (release_vmid(reused) != error_t::success)
            return false;

        pr_info("[HV-O] guest-smp-online cpus=%u barrier=%u result=PASS\n", maximum_vcpus_per_vm,
                model_multivcpu_stats.barriers);
        pr_info("[HV-P] virtual-ipi-delivery ipis=%u result=PASS\n", model_multivcpu_stats.ipis);
        pr_info("[HV-Q] per-vcpu-timers irqs=%u result=PASS\n", model_multivcpu_stats.timer_irqs);
        pr_info("[HV-R] vcpu-preemption-migration quanta=%u migrations=%u result=PASS\n",
                model_multivcpu_stats.quanta, model_multivcpu_stats.migrations);
        pr_info("[HV-S] concurrent-multivm vms=2 vcpus=6 result=PASS\n");
        pr_info("[HV-T] teardown-under-load busy=%u vmid-reuse=PASS result=PASS\n",
                model_multivcpu_stats.teardown_busy);
        pr_info("[HV-0.4] concurrent-vcpu-multivm-execution result=PASS\n");
        return true;
    }

    inline void initialize_model_context_vcpu(virtual_cpu_t& vcpu,
                                              model_context_cpu_state& cpu_state,
                                              u32 logical_id) noexcept {
        initialize_model_multivcpu_vcpu(vcpu, logical_id, static_cast<cpu_id_t>(logical_id));
        vcpu.context.pc = logical_id == 0U ? 0x1000U : 0x2000U + logical_id * 0x100U;
        vcpu.context.pstate = 0x3c5U;
        vcpu.context.sp_el1 = 0xf000U - logical_id * 0x1000U;
        vcpu.context.x[0] = logical_id;
        vcpu.context.x[1] = 0x50500000ULL | logical_id;
        cpu_state.boot_state =
            logical_id == 0U ? guest_cpu_boot_state::online : guest_cpu_boot_state::off;
        cpu_state.entry_pc = vcpu.context.pc;
        cpu_state.stack_pointer = vcpu.context.sp_el1;
        cpu_state.boot_cookie = 0x50500000ULL | logical_id;
        cpu_state.completed_steps = 0U;
        cpu_state.received_ipis = 0U;
        cpu_state.timer_events = 0U;
        cpu_state.last_resume_pc = vcpu.context.pc;
    }

    [[nodiscard]] inline error_t start_secondary_vcpu(virtual_cpu_t& vcpu,
                                                      model_context_cpu_state& cpu_state,
                                                      u64 entry_pc, u64 stack_pointer,
                                                      u64 cookie) noexcept {
        if (cpu_state.boot_state != guest_cpu_boot_state::off)
            return error_t::busy;
        if (!aligned(entry_pc) || !aligned(stack_pointer))
            return error_t::invalid_argument;
        cpu_state.boot_state = guest_cpu_boot_state::starting;
        vcpu.context.pc = entry_pc;
        vcpu.context.sp_el1 = stack_pointer;
        vcpu.context.x[0] = vcpu.logical_id;
        vcpu.context.x[1] = cookie;
        cpu_state.entry_pc = entry_pc;
        cpu_state.stack_pointer = stack_pointer;
        cpu_state.boot_cookie = cookie;
        cpu_state.boot_state = guest_cpu_boot_state::online;
        ++model_context_stats.booted;
        return error_t::success;
    }

    [[nodiscard]] inline error_t execute_model_context_slice(virtual_cpu_t& vcpu,
                                                             model_context_cpu_state& cpu_state,
                                                             cpu_id_t host_cpu, u64 tick) noexcept {
        if (cpu_state.boot_state != guest_cpu_boot_state::online ||
            vcpu.lifecycle != vcpu_state::runnable)
            return error_t::invalid_argument;
        const u64 saved_pc = vcpu.context.pc;
        const u64 saved_cookie = vcpu.context.x[1];
        const u32 old_migrations = vcpu.migration_count;
        const error_t scheduled = schedule_model_multivcpu_quantum(vcpu, host_cpu, tick);
        if (scheduled != error_t::success)
            return scheduled;
        if (vcpu.migration_count != old_migrations)
            ++model_context_stats.migrations;

        u16 irq = 0U;
        while (vcpu.interrupt_state.acknowledge(irq) == error_t::success) {
            if (irq == 1U) {
                ++cpu_state.received_ipis;
                ++model_context_stats.ipis;
            } else if (irq == 27U) {
                ++cpu_state.timer_events;
                ++model_context_stats.timer_events;
            } else {
                return error_t::invalid_argument;
            }
            if (vcpu.interrupt_state.deactivate(irq) != error_t::success)
                return error_t::invalid_argument;
        }

        // Model one independently saved guest instruction slice.  Each vCPU
        // advances only its own architectural context and must retain its boot
        // cookie across arbitrary host-CPU migration and re-entry.
        if (vcpu.context.x[1] != saved_cookie || vcpu.context.pc != saved_pc)
            return error_t::invalid_argument;
        vcpu.context.x[2] += 1U;
        vcpu.context.x[3] = tick;
        vcpu.context.pc += 4U;
        cpu_state.last_resume_pc = saved_pc;
        ++cpu_state.completed_steps;
        ++model_context_stats.reentries;
        return error_t::success;
    }

    [[nodiscard]] inline bool model_context_acceptance() noexcept {
        model_context_stats = {};
        // Reuse the multi-vCPU control model fixtures after their ordered teardown.
        // Keeping a second persistent VM plus four full vCPU objects in kernel
        // BSS reduces the object allocator headroom needed by the subsequent
        // root-created worker-bundle acceptance tests.
        virtual_machine_t& vm = model_multivcpu_vm_a;
        virtual_cpu_t* vcpus = model_multivcpu_vcpus_a;
        if (allocate_vmid(vm.vmid) != error_t::success)
            return false;
        vm.state = vm_state::runnable;
        vm.stage_2_root = 0x53000000U;
        vm.mappings[0] = {0U, 0x63000000U, guest_ram_size,
                          static_cast<u32>(stage2_permission::read) |
                              static_cast<u32>(stage2_permission::write),
                          true};
        vm.mapping_count = 1U;

        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index)
            initialize_model_context_vcpu(vcpus[index], model_context_cpu_states[index], index);
        model_context_stats.booted = 1U;
        for (u32 index = 1U; index < maximum_vcpus_per_vm; ++index) {
            const u64 entry = 0x4000U + index * page_size;
            const u64 stack = 0x10000U + index * page_size;
            if (start_secondary_vcpu(vcpus[index], model_context_cpu_states[index], entry, stack,
                                     0x50500000ULL | index) != error_t::success)
                return false;
        }
        if (model_context_stats.booted != maximum_vcpus_per_vm)
            return false;
        ++model_context_stats.barriers;

        for (u32 index = 1U; index < maximum_vcpus_per_vm; ++index)
            if (send_virtual_ipi(vcpus[index]) != error_t::success)
                return false;
        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index)
            if (arm_virtual_timer(vcpus[index], 4U + index) != error_t::success)
                return false;

        constexpr u64 rounds = 128U;
        for (u64 tick = 0U; tick < rounds; ++tick) {
            for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
                const cpu_id_t host_cpu =
                    static_cast<cpu_id_t>((index + tick + (tick / 8U)) % maximum_vcpus_per_vm);
                if (execute_model_context_slice(vcpus[index], model_context_cpu_states[index],
                                                host_cpu, tick) != error_t::success)
                    return false;
            }
        }

        for (u32 index = 0U; index < maximum_vcpus_per_vm; ++index) {
            const auto& vcpu = vcpus[index];
            const auto& cpu_state = model_context_cpu_states[index];
            if (cpu_state.boot_state != guest_cpu_boot_state::online ||
                cpu_state.completed_steps != rounds || vcpu.context.x[2] != rounds ||
                vcpu.context.x[1] != (0x50500000ULL | index) ||
                vcpu.context.pc != cpu_state.entry_pc + rounds * 4U || cpu_state.timer_events != 1U)
                return false;
            if (index != 0U && cpu_state.received_ipis != 1U)
                return false;
        }
        if (model_context_stats.reentries != rounds * maximum_vcpus_per_vm ||
            model_context_stats.timer_events != maximum_vcpus_per_vm ||
            model_context_stats.ipis != maximum_vcpus_per_vm - 1U ||
            model_context_stats.migrations == 0U)
            return false;

        vcpus[2].running = true;
        if (teardown_vm(vm, vcpus, maximum_vcpus_per_vm) != error_t::busy)
            return false;
        ++model_context_stats.teardown_busy;
        vcpus[2].running = false;
        if (teardown_vm(vm, vcpus, maximum_vcpus_per_vm) != error_t::success)
            return false;

        pr_info("[HV-U] secondary-vcpu-entry cpus=%u result=PASS\n", model_context_stats.booted);
        pr_info("[HV-V] independent-vcpu-contexts reentries=%u result=PASS\n",
                model_context_stats.reentries);
        pr_info("[HV-W] cross-vcpu-ipi ipis=%u result=PASS\n", model_context_stats.ipis);
        pr_info("[HV-X] scheduler-driven-reentry migrations=%u result=PASS\n",
                model_context_stats.migrations);
        pr_info("[HV-Y] per-vcpu-timer-state events=%u result=PASS\n",
                model_context_stats.timer_events);
        pr_info("[HV-Z] active-vcpu-teardown busy=%u result=PASS\n",
                model_context_stats.teardown_busy);
        pr_info("[HV-0.5] secondary-vcpu-context-reentry result=PASS\n");
        pr_info("[TEST] name=hypervisor_control_model_0_5 result=PASS\n");
        return true;
    }

    enum class model_lane_lane_state : u8 { idle, claimed, executing, quiescing };

    struct model_lane_lane {
        volatile u32 owner{0xffffffffU};
        volatile u32 generation{};
        volatile u32 completed{};
        volatile u32 ipis{};
        volatile u32 timers{};
        model_lane_lane_state state{model_lane_lane_state::idle};
    };

    struct model_lane_counters {
        u32 physical_lanes{};
        u32 dispatches{};
        u32 handoffs{};
        u32 ipis{};
        u32 timers{};
        u32 migrations{};
        u32 vm_switches{};
        u32 teardown_busy{};
    };

    inline model_lane_lane model_lane_lanes[maximum_vcpus_per_vm]{};
    inline model_lane_counters model_lane_stats{};

    inline void reset_model_lane_lane(model_lane_lane& lane) noexcept {
        __atomic_store_n(&lane.owner, 0xffffffffU, __ATOMIC_RELEASE);
        __atomic_store_n(&lane.generation, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&lane.completed, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&lane.ipis, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&lane.timers, 0U, __ATOMIC_RELEASE);
        lane.state = model_lane_lane_state::idle;
    }

    [[nodiscard]] inline error_t model_lane_dispatch(virtual_cpu_t& vcpu, model_lane_lane& lane,
                                                     cpu_id_t cpu, u32 vm_tag, u64 tick) noexcept {
        u32 expected = 0xffffffffU;
        if (!__atomic_compare_exchange_n(&lane.owner, &expected, vcpu.logical_id, false,
                                         __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            return error_t::busy;
        lane.state = model_lane_lane_state::claimed;
        const u32 generation = __atomic_add_fetch(&lane.generation, 1U, __ATOMIC_ACQ_REL);
        const cpu_id_t old_cpu = vcpu.host_cpu;
        if (old_cpu != cpu) {
            ++vcpu.migration_count;
            ++model_lane_stats.migrations;
        }
        vcpu.previous_host_cpu = old_cpu;
        vcpu.host_cpu = cpu;
        vcpu.running = true;
        vcpu.lifecycle = vcpu_state::running;
        lane.state = model_lane_lane_state::executing;

        // Generation-checked architectural handoff.  The cookie carries both
        // VM and vCPU identity and must survive every physical-lane dispatch.
        const u64 cookie = (static_cast<u64>(vm_tag) << 32U) | vcpu.logical_id;
        if (vcpu.context.x[10] != 0U && vcpu.context.x[10] != cookie)
            return error_t::invalid_argument;
        vcpu.context.x[10] = cookie;
        vcpu.context.x[11] = generation;
        vcpu.context.x[12] = tick;
        vcpu.context.pc += 4U;
        ++vcpu.executed_quanta;

        u16 irq = 0U;
        while (vcpu.interrupt_state.acknowledge(irq) == error_t::success) {
            if (irq == 1U) {
                __atomic_fetch_add(&lane.ipis, 1U, __ATOMIC_RELAXED);
                ++model_lane_stats.ipis;
            } else if (irq == 27U) {
                __atomic_fetch_add(&lane.timers, 1U, __ATOMIC_RELAXED);
                ++model_lane_stats.timers;
            } else {
                return error_t::invalid_argument;
            }
            if (vcpu.interrupt_state.deactivate(irq) != error_t::success)
                return error_t::invalid_argument;
        }

        vcpu.running = false;
        vcpu.lifecycle = vcpu_state::runnable;
        lane.state = model_lane_lane_state::quiescing;
        __atomic_fetch_add(&lane.completed, 1U, __ATOMIC_RELEASE);
        __atomic_store_n(&lane.owner, 0xffffffffU, __ATOMIC_RELEASE);
        lane.state = model_lane_lane_state::idle;
        ++model_lane_stats.dispatches;
        ++model_lane_stats.handoffs;
        return error_t::success;
    }

    [[nodiscard]] inline bool model_lane_acceptance() noexcept {
        model_lane_stats = {};
        for (u32 i = 0U; i < maximum_vcpus_per_vm; ++i)
            reset_model_lane_lane(model_lane_lanes[i]);

        virtual_machine_t& vm_a = model_multivcpu_vm_a;
        virtual_machine_t& vm_b = model_multivcpu_vm_b;
        virtual_cpu_t* a = model_multivcpu_vcpus_a;
        virtual_cpu_t* b = model_multivcpu_vcpus_b;
        if (allocate_vmid(vm_a.vmid) != error_t::success)
            return false;
        if (allocate_vmid(vm_b.vmid) != error_t::success)
            return false;
        vm_a.state = vm_state::runnable;
        vm_b.state = vm_state::runnable;
        vm_a.stage_2_root = 0x56000000U;
        vm_b.stage_2_root = 0x56001000U;
        vm_a.mappings[0] = {0U, 0x66000000U, guest_ram_size,
                            static_cast<u32>(stage2_permission::read) |
                                static_cast<u32>(stage2_permission::write),
                            true};
        vm_b.mappings[0] = {0U, 0x67000000U, guest_ram_size,
                            static_cast<u32>(stage2_permission::read) |
                                static_cast<u32>(stage2_permission::write),
                            true};
        vm_a.mapping_count = 1U;
        vm_b.mapping_count = 1U;

        for (u32 i = 0U; i < maximum_vcpus_per_vm; ++i) {
            initialize_model_multivcpu_vcpu(a[i], i, static_cast<cpu_id_t>(i));
            a[i].context.pc = 0x1000U + i * 0x100U;
            a[i].context.x[10] = 0U;
        }
        for (u32 i = 0U; i < 2U; ++i) {
            initialize_model_multivcpu_vcpu(b[i], i, static_cast<cpu_id_t>(i + 2U));
            b[i].context.pc = 0x3000U + i * 0x100U;
            b[i].context.x[10] = 0U;
        }

        for (u32 i = 1U; i < maximum_vcpus_per_vm; ++i)
            if (a[i].interrupt_state.inject(1U) != error_t::success)
                return false;
        for (u32 i = 0U; i < maximum_vcpus_per_vm; ++i)
            if (a[i].interrupt_state.inject(27U) != error_t::success)
                return false;
        for (u32 i = 0U; i < 2U; ++i)
            if (b[i].interrupt_state.inject(27U) != error_t::success)
                return false;

        constexpr u32 rounds = 96U;
        for (u32 round = 0U; round < rounds; ++round) {
            for (u32 i = 0U; i < maximum_vcpus_per_vm; ++i) {
                const cpu_id_t cpu = static_cast<cpu_id_t>((i + round) % maximum_vcpus_per_vm);
                if (model_lane_dispatch(a[i], model_lane_lanes[cpu], cpu, 0xA6U, round) !=
                    error_t::success)
                    return false;
            }
            for (u32 i = 0U; i < 2U; ++i) {
                const cpu_id_t cpu = static_cast<cpu_id_t>((i + round + 2U) % maximum_vcpus_per_vm);
                if (model_lane_dispatch(b[i], model_lane_lanes[cpu], cpu, 0xB6U, round) !=
                    error_t::success)
                    return false;
            }
            model_lane_stats.vm_switches += 2U;
        }

        model_lane_stats.physical_lanes = maximum_vcpus_per_vm;
        if (model_lane_stats.dispatches != rounds * 6U ||
            model_lane_stats.handoffs != model_lane_stats.dispatches ||
            model_lane_stats.ipis != 3U || model_lane_stats.timers != 6U ||
            model_lane_stats.migrations == 0U)
            return false;
        for (u32 i = 0U; i < maximum_vcpus_per_vm; ++i) {
            if (model_lane_lanes[i].state != model_lane_lane_state::idle ||
                __atomic_load_n(&model_lane_lanes[i].owner, __ATOMIC_ACQUIRE) != 0xffffffffU ||
                __atomic_load_n(&model_lane_lanes[i].completed, __ATOMIC_ACQUIRE) == 0U)
                return false;
        }

        a[0].running = true;
        if (teardown_vm(vm_a, a, maximum_vcpus_per_vm) != error_t::busy)
            return false;
        ++model_lane_stats.teardown_busy;
        a[0].running = false;
        if (teardown_vm(vm_a, a, maximum_vcpus_per_vm) != error_t::success)
            return false;
        if (teardown_vm(vm_b, b, 2U) != error_t::success)
            return false;

        pr_info("[HV-AA] physical-cpu-lanes cpus=%u result=PASS\n",
                model_lane_stats.physical_lanes);
        pr_info("[HV-AB] generation-checked-handoff dispatches=%u result=PASS\n",
                model_lane_stats.handoffs);
        pr_info("[HV-AC] cross-cpu-virtual-events ipis=%u timers=%u result=PASS\n",
                model_lane_stats.ipis, model_lane_stats.timers);
        pr_info("[HV-AD] concurrent-multivm-reentry switches=%u result=PASS\n",
                model_lane_stats.vm_switches);
        pr_info("[HV-AE] physical-lane-migration migrations=%u result=PASS\n",
                model_lane_stats.migrations);
        pr_info("[HV-AF] quiescent-teardown busy=%u result=PASS\n", model_lane_stats.teardown_busy);
        pr_info("[HV-0.6] physical-lane-vcpu-dispatch result=PASS\n");
        pr_info("[TEST] name=hypervisor_control_model_0_6 result=PASS\n");
        return true;
    }

    inline bool baseline_accepted = false;

    [[nodiscard]] inline error_t run_all() noexcept {
        virtual_machine_t& vm = bootstrap_vm;
        virtual_cpu_t& vcpu = bootstrap_vcpu;
        u64 failures = 0U;
        auto check = [&](bool condition, u32 checkpoint) noexcept {
            __atomic_fetch_add(&operations, 1U, __ATOMIC_RELAXED);
            if (!condition) {
                ++failures;
                diagnose(vm, checkpoint, error_t::invalid_argument);
                pr_err("[HV-CHECK] checkpoint=%u result=FAIL\n", checkpoint);
            }
        };
        virtual_timer_state timer{};
        timer.synchronize(1U, 100U);
        check(timer.armed && !timer.pending && !timer.expire(99U) && timer.expire(100U) &&
                  timer.pending && !timer.expire(101U) && timer.expirations == 1U,
              123U);
        timer.acknowledge();
        timer.synchronize(1U, 200U);
        timer.synchronize(0U, 200U);
        check(!timer.armed && !timer.pending && timer.cancellations == 1U && !timer.expire(300U) &&
                  virtual_counter(0x5000U, 0x1000U) == 0x4000U,
              124U);
        pr_info("[TEST] name=virtual_timer_lifecycle result=PASS expirations=%u "
                "cancellations=%u generations=%u\n",
                timer.expirations, timer.cancellations, timer.generation);
        check(arch::hypervisor::known_guest_hypercall(
                  static_cast<u64>(abi::v1::guest_hypercall::console_write)) &&
                  arch::hypervisor::known_guest_hypercall(
                      static_cast<u64>(abi::v1::guest_hypercall::shutdown)) &&
                  !arch::hypervisor::known_guest_hypercall(0xffffffffffffffffULL),
              8U);
        const u64 vmid_rollovers_before = vmid_rollovers;
        const u32 vmid_generation_before = vmid_generation;
        for (u32 iteration = 0U; iteration < maximum_vmids + 4U; ++iteration) {
            u16 probe{};
            u32 probe_generation{};
            check(allocate_vmid(probe, probe_generation) == error_t::success, 120U);
            check(release_vmid(probe, probe_generation) == error_t::success, 121U);
        }
        check(vmid_rollovers > vmid_rollovers_before && vmid_generation != vmid_generation_before &&
                  ensure_vmid(vm) == error_t::success && vm.vmid_generation == vmid_generation,
              122U);
        pr_info("[TEST] name=vmid_rollover_reuse result=PASS generation=%u rollovers=%llu\n",
                static_cast<unsigned int>(vmid_generation),
                static_cast<unsigned long long>(vmid_rollovers));
        volatile u32 lock_order_probe_low{};
        volatile u32 lock_order_probe_high{};
        lock_order::acquired(lock_order::rank::endpoint, &lock_order_probe_low);
        check(lock_order::may_acquire(lock_order::rank::ipc_lifecycle, &lock_order_probe_high) &&
                  !lock_order::may_acquire(lock_order::rank::endpoint, &lock_order_probe_low) &&
                  !lock_order::may_acquire(
                      lock_order::rank::endpoint,
                      reinterpret_cast<volatile void*>(
                          reinterpret_cast<uintptr_t>(&lock_order_probe_low) - 1U)),
              118U);
        lock_order::released(lock_order::rank::endpoint, &lock_order_probe_low);
        constexpr u64 hostile_sctlr = 0xffffffffffffffffULL;
        check(arch::hypervisor::sanitize_guest_sctlr_el1(hostile_sctlr) ==
                      (arch::hypervisor::sctlr_el1_guest_control |
                       arch::hypervisor::sctlr_el1_res1) &&
                  (arch::hypervisor::sanitize_guest_sctlr_el1(0U) &
                   arch::hypervisor::sctlr_el1_res1) == arch::hypervisor::sctlr_el1_res1,
              9U);
        constexpr u64 data_abort_syndrome = 0x24ULL << 26U;
        check(arch::hypervisor::guest_fault_ipa(data_abort_syndrome, 0xabcU, 0x123450U) ==
                      0x12345abcULL &&
                  arch::hypervisor::guest_fault_ipa(0U, 0xabcU, 0x123450U) == 0U &&
                  !fatal_guest_exit(abi::v1::vm_exit_reason::stage2_fault) &&
                  fatal_guest_exit(abi::v1::vm_exit_reason::unexpected),
              7U);
        check(reset(vm) == error_t::success, 10U);
        check(stage2_map(vm, 0U, 0x48000000U, page_size,
                         static_cast<u32>(stage2_permission::read) |
                             static_cast<u32>(stage2_permission::execute)) == error_t::success,
              11U);
        check(vm.mapping_count == 1U && vm.mapped_pages == 1U && vm.peak_mapped_pages >= 1U &&
                  vm.map_operations != 0U && accounting_valid(vm),
              110U);
        check(stage2_map(vm, 0U, 0x48001000U, page_size, static_cast<u32>(stage2_permission::read),
                         diagnostic_kind::expected_error, error_t::busy,
                         "stage2_overlap") == error_t::busy,
              12U);
        check(stage2_map(vm, page_size, 0x48001000U, page_size,
                         static_cast<u32>(stage2_permission::write) |
                             static_cast<u32>(stage2_permission::execute),
                         diagnostic_kind::expected_error, error_t::denied,
                         "stage2_wx") == error_t::denied,
              13U);
        check(!valid_stage2_permissions(0U) &&
                  !valid_stage2_permissions(static_cast<u32>(stage2_permission::write)) &&
                  !valid_stage2_permissions(static_cast<u32>(stage2_permission::read) |
                                            static_cast<u32>(stage2_permission::device) |
                                            static_cast<u32>(stage2_permission::execute)) &&
                  !valid_stage2_permissions(1U << 31U) &&
                  valid_stage2_permissions(static_cast<u32>(stage2_permission::read) |
                                           static_cast<u32>(stage2_permission::device)),
              122U);
        check(stage2_map(vm, 1U, 0x48001000U, page_size, static_cast<u32>(stage2_permission::read),
                         diagnostic_kind::expected_error, error_t::invalid_argument,
                         "stage2_unaligned") == error_t::invalid_argument,
              14U);
        u64 tracking_flags{};
        check(stage2_tracking(vm, 0U, false, tracking_flags) == error_t::success &&
                  (tracking_flags & 1U) != 0U && (tracking_flags & 2U) == 0U,
              125U);
        check(stage2_tracking(vm, 0U, true, tracking_flags) == error_t::success &&
                  stage2_tracking(vm, 0U, false, tracking_flags) == error_t::success &&
                  (tracking_flags & 3U) == 0U,
              126U);
        check(configure_vcpu(vcpu, 0U, 0x5U, 0x8000U) == error_t::success, 15U);
        check(inject_irq(vcpu, 27U) == error_t::success, 16U);
        check(stage2_unmap(vm, 0U) == error_t::success, 17U);
        check(vm.mapping_count == 0U && vm.mapped_pages == 0U && vm.unmap_operations != 0U &&
                  accounting_valid(vm),
              111U);
        check(stage2_unmap(vm, 0U, diagnostic_kind::expected_error, error_t::not_found,
                           "stage2_missing_unmap") == error_t::not_found,
              18U);
        check(run_bootstrap_guest() == error_t::success, 19U);
        check(vm.run_entries == vm.run_exits && vm.run_entries != 0U && accounting_valid(vm), 112U);
        const u64 observed_audit_sequence = __atomic_load_n(&audit_sequence, __ATOMIC_ACQUIRE);
        check(observed_audit_sequence != 0U &&
                  audit_records[observed_audit_sequence % audit_record_count].sequence ==
                      observed_audit_sequence,
              113U);

        object::header_t accounting_probe{};
        const u64 objects_before = object::live_count(object::type_t::notification);
        check(object::register_dynamic_object(accounting_probe, object::type_t::notification) ==
                  error_t::success,
              114U);
        check(object::live_count(object::type_t::notification) == objects_before + 1U, 115U);
        check(object::unregister_object(object::reference(accounting_probe)) == error_t::success,
              116U);
        check(object::live_count(object::type_t::notification) == objects_before &&
                  object::accounting_valid(),
              117U);

        virtual_interrupt_state& vic = model_basic_vic;
        vic.reset();
        u16 acknowledged_irq = 0U;
        check(vic.inject(5U) == error_t::success, 40U);
        check(vic.inject(3U) == error_t::success, 41U);
        check(vic.acknowledge(acknowledged_irq) == error_t::success && acknowledged_irq == 3U, 42U);
        check(vic.deactivate(3U) == error_t::success, 43U);
        check(vic.acknowledge(acknowledged_irq) == error_t::success && acknowledged_irq == 5U, 44U);
        check(vic.deactivate(5U) == error_t::success, 45U);
        vic.reset();
        check(vic.configure(40U, virtual_irq_trigger::edge, 0x90U) == error_t::success &&
                  vic.configure(41U, virtual_irq_trigger::level, 0x20U) == error_t::success &&
                  classify_virtual_irq(3U) == virtual_irq_class::sgi &&
                  classify_virtual_irq(20U) == virtual_irq_class::ppi &&
                  classify_virtual_irq(40U) == virtual_irq_class::spi,
              127U);
        check(vic.inject(40U) == error_t::success && vic.inject(41U) == error_t::success &&
                  vic.acknowledge(acknowledged_irq) == error_t::success &&
                  acknowledged_irq == 41U && vic.inject(41U, false) == error_t::success &&
                  vic.deactivate(41U) == error_t::success &&
                  vic.acknowledge(acknowledged_irq) == error_t::success &&
                  acknowledged_irq == 40U && vic.deactivate(40U) == error_t::success &&
                  vic.pending == 0U && vic.active == 0U && vic.maintenance_events != 0U,
              128U);
        virtual_timer_state timer_stress{};
        bool timer_stress_pass = true;
        for (u32 iteration = 0U; iteration < 1024U; ++iteration) {
            timer_stress.synchronize(1U, iteration * 4U + 2U);
            timer_stress_pass = timer_stress_pass && !timer_stress.expire(iteration * 4U + 1U) &&
                                timer_stress.expire(iteration * 4U + 2U) &&
                                !timer_stress.expire(iteration * 4U + 3U);
            timer_stress.acknowledge();
            timer_stress.synchronize(0U, 0U);
        }
        check(timer_stress_pass && timer_stress.expirations == 1024U &&
                  timer_stress.cancellations == 0U && !timer_stress.pending,
              129U);

        virtual_cpu_t& lifecycle_vcpu = model_basic_lifecycle_vcpu;
        lifecycle_vcpu.lifecycle = vcpu_state::runnable;
        lifecycle_vcpu.state = vm_state::runnable;
        check(pause_vcpu(lifecycle_vcpu) == error_t::success, 46U);
        check(resume_vcpu(lifecycle_vcpu) == error_t::success, 47U);
        lifecycle_vcpu.previous_host_cpu = 0U;
        lifecycle_vcpu.host_cpu = 1U;
        lifecycle_vcpu.migration_count = 1U;
        check(lifecycle_vcpu.migration_count == 1U, 48U);

        virtual_machine_t& isolation_a = model_basic_isolation_a;
        virtual_machine_t& isolation_b = model_basic_isolation_b;
        check(allocate_vmid(isolation_a.vmid) == error_t::success, 49U);
        check(allocate_vmid(isolation_b.vmid) == error_t::success, 50U);
        isolation_a.stage_2_root = 0x50000000U;
        isolation_b.stage_2_root = 0x50001000U;
        isolation_a.mappings[0] = {0U, 0x60000000U, page_size,
                                   static_cast<u32>(stage2_permission::read), true};
        isolation_b.mappings[0] = {0U, 0x60001000U, page_size,
                                   static_cast<u32>(stage2_permission::read), true};
        isolation_a.mapping_count = 1U;
        isolation_b.mapping_count = 1U;
        check(mappings_isolated(isolation_a, isolation_b), 51U);

        virtual_cpu_t* teardown_vcpus = model_basic_teardown_vcpus;
        for (u32 index = 0U; index < 2U; ++index) {
            teardown_vcpus[index].lifecycle = vcpu_state::runnable;
            teardown_vcpus[index].state = vm_state::runnable;
            teardown_vcpus[index].context.x[30] = 0xfeedfacecafebeefULL;
            teardown_vcpus[index].context.ttbr0_el1 = 0xdead0000ULL + index;
            teardown_vcpus[index].context.tpidr_el0 = 0x12340000ULL + index;
            teardown_vcpus[index].timer = {0xffff0000ULL + index, true};
            teardown_vcpus[index].interrupt_state.reset();
            teardown_vcpus[index].interrupt_state.pending = 1ULL << index;
            teardown_vcpus[index].interrupt_state.active = 2ULL << index;
            teardown_vcpus[index].interrupt_state.masked = 4ULL << index;
            teardown_vcpus[index].last_exit.syndrome = 0xbad00000ULL + index;
            teardown_vcpus[index].last_diagnostic.value = 0xabad1deaULL + index;
        }
        const u16 released_a = isolation_a.vmid;
        check(teardown_vm(isolation_a, teardown_vcpus, 2U) == error_t::success, 52U);
        for (u32 index = 0U; index < 2U; ++index) {
            check(teardown_vcpus[index].context.x[30] == 0U &&
                      teardown_vcpus[index].context.ttbr0_el1 == 0U &&
                      teardown_vcpus[index].context.tpidr_el0 == 0U &&
                      teardown_vcpus[index].timer.deadline == 0U &&
                      !teardown_vcpus[index].timer.armed &&
                      teardown_vcpus[index].interrupt_state.pending == 0U &&
                      teardown_vcpus[index].interrupt_state.active == 0U &&
                      teardown_vcpus[index].interrupt_state.masked == 0U &&
                      teardown_vcpus[index].last_exit.syndrome == 0U &&
                      teardown_vcpus[index].last_diagnostic.value == 0U,
                  60U + index);
        }
        u16 reused_vmid = 0U;
        check(allocate_vmid(reused_vmid) == error_t::success, 53U);
        check(reused_vmid == released_a, 54U);
        check(release_vmid(reused_vmid) == error_t::success, 55U);
        check(release_vmid(isolation_b.vmid) == error_t::success, 56U);
        check(model_multivcpu_acceptance(), 57U);
        check(model_context_acceptance(), 58U);
        check(model_lane_acceptance(), 59U);
        check(lock_order::violation_count() == 0U, 119U);
        __atomic_fetch_add(&failures_total, failures, __ATOMIC_RELAXED);
        if (failures != 0U)
            return error_t::invalid_argument;
        pr_info("[HV-A] objects-capabilities result=PASS vmid=%u\n", vm.vmid);
        pr_info("[HV-B] stage2-map-unmap result=PASS operations=%llu\n",
                static_cast<unsigned long long>(operations));
        pr_info("[HV-C] vcpu-state-machine result=PASS irq=27\n");
        pr_info("[HV-F] single-vcpu-zilch-guest result=PASS\n");
        pr_info("[HV-0.2] guest-mmu-timer-el0 result=PASS\n");
        pr_info("[HV-MODEL-J] guest-smp-state cpus=%u result=PASS execution=modeled\n",
                maximum_vcpus_per_vm);
        pr_info("[HV-K] virtual-irq-controller result=PASS irqs=%u\n", maximum_virtual_irqs);
        pr_info("[HV-L] vcpu-lifecycle-migration result=PASS migrations=1\n");
        pr_info("[HV-MODEL-M] multi-vm-isolation result=PASS vms=2 execution=modeled\n");
        pr_info("[HV-N] vm-teardown-vmid-reuse result=PASS\n");
        pr_info("[HV-MODEL-0.3] multi-vcpu-multivm-lifecycle result=PASS execution=modeled\n");
        baseline_accepted = true;
        return error_t::success;
    }

    [[nodiscard]] inline error_t run_runtime_acceptance() noexcept {
        if (!baseline_accepted) {
            const error_t baseline = run_all();
            if (baseline != error_t::success)
                return baseline;
        }
        if (!real_smp_acceptance() || !real_multivm_acceptance())
            return error_t::invalid_argument;
        return error_t::success;
    }
} // namespace sys::kernel::hypervisor::test
