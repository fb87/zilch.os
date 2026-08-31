#pragma once

#include <sys/arch/timer.hh>
#include <sys/kernel/hypervisor/object.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::hypervisor
{
    inline constexpr u32 vcpu_state_field_count = 48U;

    [[nodiscard]] inline error_t vcpu_state_access(virtual_cpu_t& vcpu, u32 field, bool write,
                                                   u64& value) noexcept {
        if (vcpu.running || field >= vcpu_state_field_count)
            return vcpu.running ? error_t::busy : error_t::invalid_argument;
        if (field < 31U) {
            if (write)
                vcpu.context.x[field] = value;
            else
                value = vcpu.context.x[field];
            return error_t::success;
        }
        u64* target = nullptr;
        switch (field) {
            case 31U:
                target = &vcpu.context.pc;
                break;
            case 32U:
                target = &vcpu.context.pstate;
                break;
            case 33U:
                target = &vcpu.context.sp_el0;
                break;
            case 34U:
                target = &vcpu.context.sp_el1;
                break;
            case 35U:
                target = &vcpu.context.elr_el1;
                break;
            case 36U:
                target = &vcpu.context.spsr_el1;
                break;
            case 37U:
                target = &vcpu.context.sctlr_el1;
                break;
            case 38U:
                target = &vcpu.context.tcr_el1;
                break;
            case 39U:
                target = &vcpu.context.ttbr0_el1;
                break;
            case 40U:
                target = &vcpu.context.ttbr1_el1;
                break;
            case 41U:
                target = &vcpu.context.mair_el1;
                break;
            case 42U:
                target = &vcpu.context.vbar_el1;
                break;
            case 43U:
                target = &vcpu.context.tpidr_el0;
                break;
            case 44U:
                target = &vcpu.context.tpidr_el1;
                break;
            case 45U:
                target = &vcpu.context.cntv_ctl_el0;
                break;
            case 46U:
                target = &vcpu.context.cntv_cval_el0;
                break;
            case 47U:
                target = &vcpu.context.contextidr_el1;
                break;
            default:
                return error_t::invalid_argument;
        }
        if (!write) {
            value = *target;
            return error_t::success;
        }
        /*
         * PC only needs 4-byte AArch64 instruction alignment (matching
         * configure_vcpu()'s own (pc & 3U) check below), not the page
         * alignment aligned() checks (correct for the TTBR fields below,
         * which do need it) -- using aligned() here rejected every
         * legitimate post-emulation PC advance (e.g. guest_pc + 4), since
         * an instruction address is essentially never page-aligned. Latent
         * until now: nothing called vcpu_state_write for field 31 before
         * MMIO-exit resolve-and-resume needed to advance PC.
         */
        if (field == 31U && (value & 3U) != 0U)
            return error_t::invalid_argument;
        if (field == 32U)
            value = arch::hypervisor::sanitize_guest_pstate(value);
        else if (field == 37U)
            value = arch::hypervisor::sanitize_guest_sctlr_el1(value);
        else if (field == 38U)
            value = arch::hypervisor::sanitize_guest_tcr_el1(value);
        else if ((field == 39U || field == 40U || field == 42U) && !aligned(value))
            return error_t::invalid_argument;
        *target = value;
        return error_t::success;
    }

    [[nodiscard]] inline constexpr bool fatal_guest_exit(abi::v1::vm_exit_reason reason) noexcept {
        return reason == abi::v1::vm_exit_reason::unexpected;
    }

    [[nodiscard]] inline error_t configure_vcpu(virtual_cpu_t& vcpu, u64 pc, u64 pstate,
                                                u64 sp) noexcept {
        if ((pc & 3U) != 0U || !aligned(sp))
            return error_t::invalid_argument;
        vcpu.context.pc = pc;
        vcpu.context.pstate = arch::hypervisor::sanitize_guest_pstate(pstate);
        vcpu.context.sp_el1 = sp;
        vcpu.context.sctlr_el1 = arch::hypervisor::sanitize_guest_sctlr_el1(0U);
        vcpu.context.tcr_el1 = arch::hypervisor::sanitize_guest_tcr_el1(0U);
        vcpu.context.cpacr_el1 = arch::hypervisor::sanitize_guest_cpacr_el1(0U);
        vcpu.context.cntkctl_el1 = arch::hypervisor::sanitize_guest_cntkctl_el1(0U);
        vcpu.context.gic_pmr = 0xffU;
        vcpu.context.gic_last_iar = 1023U;
        vcpu.context.gicd_ctlr = 2U;
        vcpu.context.gic_group1 = ~0ULL;
        for (u32 index = 0U; index < maximum_virtual_irqs; ++index)
            vcpu.context.gic_priority[index] = default_virtual_irq_priority;
        vcpu.interrupt_state.reset();
        vcpu.timer.reset();
        vcpu.virtual_irq_pending = false;
        vcpu.state = vm_state::runnable;
        vcpu.lifecycle = vcpu_state::runnable;
        return error_t::success;
    }

    [[nodiscard]] inline error_t inject_irq(virtual_cpu_t& vcpu, u16 irq) noexcept {
        if (vcpu.running)
            return error_t::busy;
        vcpu.virtual_irq = irq;
        const error_t irq_result = vcpu.interrupt_state.inject(irq);
        if (irq_result != error_t::success)
            return irq_result;
        __atomic_store_n(&vcpu.virtual_irq_pending, true, __ATOMIC_RELEASE);
        return error_t::success;
    }

    inline void prepare_virtual_gic(virtual_cpu_t& vcpu) noexcept {
        auto& state = vcpu.interrupt_state;
        /*
         * Do not enable the list-register interface until the guest has
         * installed an EL1 vector base and completed one bounded entry.  An
         * early pending LR would otherwise vector through uninitialized guest
         * exception state before its bootstrap masks are established.
         */
        state.hardware_active = !vcpu.context.native_gic &&
                                arch::hypervisor::virtual_gic_hardware_available() &&
                                vcpu.run_generation > 1U && vcpu.context.vbar_el1 != 0U &&
                                vcpu.virtual_irq >= 16U;
        vcpu.context.ich_hcr_el2 = vcpu.context.native_gic
                                               ? ((1ULL << 12U) | (1ULL << 10U) | 1U)
                                                             : (state.hardware_active ? 1U : 0U);
        vcpu.context.ich_vmcr_el2 = (static_cast<u64>(state.priority_mask) << 24U) |
                                    (static_cast<u64>(state.binary_point & 7U) << 18U) |
                                    (1ULL << 1U);
        for (u32 index = 0U; index < 16U; ++index)
            vcpu.context.ich_lr_el2[index] = 0U;
        if (vcpu.context.native_gic) {
            vcpu.context.gic_pending = state.pending;
            vcpu.context.gic_active = state.active;
            vcpu.context.gic_enabled = ~state.masked;
            vcpu.context.gic_group1 = ~0ULL;
            vcpu.context.gic_edge = state.edge_triggered;
            for (u32 index = 0U; index < maximum_virtual_irqs; ++index)
                vcpu.context.gic_priority[index] = state.priority[index];
            vcpu.context.gic_pmr = state.priority_mask;
            return;
        }
        if (!state.hardware_active)
            return;
        u16 irq{};
        const u64 deliverable = state.pending & ~state.masked;
        if (deliverable == 0U)
            return;
        irq = static_cast<u16>(__builtin_ctzll(deliverable));
        const u64 priority =
            state.priority[irq] == 0U ? default_virtual_irq_priority : state.priority[irq];
        constexpr u64 group1 = 1ULL << 60U;
        constexpr u64 pending = 1ULL << 62U;
        vcpu.context.ich_lr_el2[0] = static_cast<u64>(irq) | (priority << 48U) | group1 | pending;
    }

    inline void complete_virtual_gic(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.context.native_gic) {
            vcpu.interrupt_state.pending = vcpu.context.gic_pending;
            vcpu.interrupt_state.active = vcpu.context.gic_active;
            vcpu.interrupt_state.masked = ~vcpu.context.gic_enabled;
            vcpu.interrupt_state.edge_triggered = vcpu.context.gic_edge;
            vcpu.interrupt_state.priority_mask = vcpu.context.gic_pmr;
            vcpu.interrupt_state.binary_point = vcpu.context.gic_bpr;
            for (u32 index = 0U; index < maximum_virtual_irqs; ++index)
                vcpu.interrupt_state.priority[index] = vcpu.context.gic_priority[index];
            return;
        }
        if (!vcpu.interrupt_state.hardware_active)
            return;
        const u64 lr = vcpu.context.ich_lr_el2[0];
        const u16 irq = static_cast<u16>(lr & 0xffffffffU);
        const u64 state = (lr >> 62U) & 3U;
        if (irq >= maximum_virtual_irqs)
            return;
        const u64 bit = 1ULL << irq;
        if (state == 0U) {
            vcpu.interrupt_state.pending &= ~bit;
            vcpu.interrupt_state.active &= ~bit;
        } else if (state == 1U) {
            vcpu.interrupt_state.pending |= bit;
            vcpu.interrupt_state.active &= ~bit;
        } else if (state == 2U) {
            vcpu.interrupt_state.pending &= ~bit;
            vcpu.interrupt_state.active |= bit;
        } else {
            vcpu.interrupt_state.pending |= bit;
            vcpu.interrupt_state.active |= bit;
        }
    }

    [[nodiscard]] inline error_t run(virtual_cpu_t& vcpu, exit_record& exit) noexcept {
        auto* vm_header = object::resolve(vcpu.virtual_machine);
        if (vm_header == nullptr || vm_header->type != object::type_t::virtual_machine)
            return error_t::not_found;
        auto& vm = *reinterpret_cast<virtual_machine_t*>(vm_header);
        lock_vm(vm);
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success) {
            unlock_vm(vm);
            return vmid_result;
        }
        if (vcpu.state != vm_state::runnable || vm.mapping_count == 0U) {
            unlock_vm(vm);
            return error_t::invalid_argument;
        }
        vcpu.timer.synchronize(vcpu.context.cntv_ctl_el0, vcpu.context.cntv_cval_el0);
        if (vcpu.timer.expire(virtual_counter(arch::timer::counter(), vm.counter_offset))) {
            const error_t timer_irq = vcpu.interrupt_state.inject(27U);
            if (timer_irq != error_t::success) {
                unlock_vm(vm);
                return timer_irq;
            }
            vcpu.virtual_irq = 27U;
            __atomic_store_n(&vcpu.virtual_irq_pending, true, __ATOMIC_RELEASE);
        }
        if (__atomic_exchange_n(&vcpu.running, true, __ATOMIC_ACQ_REL)) {
            unlock_vm(vm);
            return error_t::busy;
        }
        if (vm.run_entries == ~static_cast<u64>(0U)) {
            ++vm.accounting_faults;
            __atomic_store_n(&vcpu.running, false, __ATOMIC_RELEASE);
            unlock_vm(vm);
            return error_t::invalid_argument;
        }
        ++vm.run_entries;
        audit(vm, audit_action::run_enter, vcpu.id);
        ++vm.active_vcpus;
        ++vcpu.run_generation;
        vcpu.previous_host_cpu = vcpu.host_cpu;
        vcpu.host_cpu = arch::cpu::current_id();
        if (vcpu.run_generation > 1U && vcpu.previous_host_cpu != vcpu.host_cpu)
            ++vcpu.migration_count;
        vcpu.lifecycle = vcpu_state::running;
        vcpu.context.pstate = arch::hypervisor::sanitize_guest_pstate(vcpu.context.pstate);
        vcpu.context.sctlr_el1 = arch::hypervisor::sanitize_guest_sctlr_el1(vcpu.context.sctlr_el1);
        vcpu.context.tcr_el1 = arch::hypervisor::sanitize_guest_tcr_el1(vcpu.context.tcr_el1);
        vcpu.context.cpacr_el1 = arch::hypervisor::sanitize_guest_cpacr_el1(vcpu.context.cpacr_el1);
        vcpu.context.cntkctl_el1 =
            arch::hypervisor::sanitize_guest_cntkctl_el1(vcpu.context.cntkctl_el1);
        const bool pending_irq = __atomic_load_n(&vcpu.virtual_irq_pending, __ATOMIC_ACQUIRE);
        prepare_virtual_gic(vcpu);
        unlock_vm(vm);
        const error_t result = arch::hypervisor::run_guest(vm.vmid, vm.stage_2_root, vcpu.context,
                                                           exit, vm.counter_offset, pending_irq);
        if (pending_irq && result == error_t::success &&
            arch::hypervisor::consume_virtual_irq_acknowledgement()) {
            __atomic_store_n(&vcpu.virtual_irq_pending, false, __ATOMIC_RELEASE);
            vcpu.interrupt_state.reset();
            vcpu.timer.acknowledge();
        }
        vcpu.timer.synchronize(vcpu.context.cntv_ctl_el0, vcpu.context.cntv_cval_el0);
        complete_virtual_gic(vcpu);
        vcpu.last_exit = exit;
        emergency::trace(emergency::event::vm_exit, vcpu.id, static_cast<u64>(exit.reason),
                         exit.syndrome, exit.qualification, exit.guest_pc);
        lock_vm(vm);
        if (vm.active_vcpus == 0U)
            ++vm.accounting_faults;
        else
            --vm.active_vcpus;
        if (vm.run_exits == ~static_cast<u64>(0U)) {
            ++vm.accounting_faults;
        } else {
            ++vm.run_exits;
        }
        audit(vm, audit_action::run_exit, vcpu.id);
        __atomic_store_n(&vcpu.running, false, __ATOMIC_RELEASE);
        unlock_vm(vm);
        const bool fatal_exit = result != error_t::success || fatal_guest_exit(exit.reason);
        vcpu.lifecycle = fatal_exit ? vcpu_state::faulted : vcpu_state::runnable;
        if (fatal_exit) {
            vcpu.state = vm_state::faulted;
            if (exit.reason == abi::v1::vm_exit_reason::unexpected)
                vm.state = vm_state::faulted;
            vcpu.last_diagnostic.result =
                result == error_t::success ? error_t::invalid_argument : result;
            vcpu.last_diagnostic.syndrome = exit.syndrome;
            vcpu.last_diagnostic.fault_address = exit.fault_address;
            vcpu.last_diagnostic.guest_pc = exit.guest_pc;
            const bool expected_negative =
                result == error_t::success && exit.reason == abi::v1::vm_exit_reason::unexpected;
#if CONFIG_VERBOSE_DIAGNOSTICS
            if (expected_negative) {
                pr_warn("hv guest-exit result=%d reason=%u esr=%llx far=%llx ipa=%llx pc=%llx "
                        "pstate=%llx vmid=%u run=%u\n",
                        static_cast<int>(result), static_cast<unsigned int>(exit.reason),
                        static_cast<unsigned long long>(exit.syndrome),
                        static_cast<unsigned long long>(exit.fault_address),
                        static_cast<unsigned long long>(exit.qualification),
                        static_cast<unsigned long long>(exit.guest_pc),
                        static_cast<unsigned long long>(vcpu.context.pstate), vm.vmid,
                        vcpu.run_generation);
                pr_warn("hv guest-regs x0=%llx x1=%llx x2=%llx x3=%llx x4=%llx x5=%llx x6=%llx "
                        "x7=%llx sp1=%llx\n",
                        static_cast<unsigned long long>(vcpu.context.x[0]),
                        static_cast<unsigned long long>(vcpu.context.x[1]),
                        static_cast<unsigned long long>(vcpu.context.x[2]),
                        static_cast<unsigned long long>(vcpu.context.x[3]),
                        static_cast<unsigned long long>(vcpu.context.x[4]),
                        static_cast<unsigned long long>(vcpu.context.x[5]),
                        static_cast<unsigned long long>(vcpu.context.x[6]),
                        static_cast<unsigned long long>(vcpu.context.x[7]),
                        static_cast<unsigned long long>(vcpu.context.sp_el1));
            } else {
                pr_err("hv guest-exit result=%d reason=%u esr=%llx far=%llx ipa=%llx pc=%llx "
                       "pstate=%llx vmid=%u run=%u\n",
                       static_cast<int>(result), static_cast<unsigned int>(exit.reason),
                       static_cast<unsigned long long>(exit.syndrome),
                       static_cast<unsigned long long>(exit.fault_address),
                       static_cast<unsigned long long>(exit.qualification),
                       static_cast<unsigned long long>(exit.guest_pc),
                       static_cast<unsigned long long>(vcpu.context.pstate), vm.vmid,
                       vcpu.run_generation);
                pr_err("hv guest-regs x0=%llx x1=%llx x2=%llx x3=%llx x4=%llx x5=%llx x6=%llx "
                       "x7=%llx sp1=%llx\n",
                       static_cast<unsigned long long>(vcpu.context.x[0]),
                       static_cast<unsigned long long>(vcpu.context.x[1]),
                       static_cast<unsigned long long>(vcpu.context.x[2]),
                       static_cast<unsigned long long>(vcpu.context.x[3]),
                       static_cast<unsigned long long>(vcpu.context.x[4]),
                       static_cast<unsigned long long>(vcpu.context.x[5]),
                       static_cast<unsigned long long>(vcpu.context.x[6]),
                       static_cast<unsigned long long>(vcpu.context.x[7]),
                       static_cast<unsigned long long>(vcpu.context.sp_el1));
            }
#else
            if (expected_negative)
                pr_warn("hv guest-exit result=%d reason=%u vmid=%u run=%u\n",
                        static_cast<int>(result), static_cast<unsigned int>(exit.reason), vm.vmid,
                        vcpu.run_generation);
            else
                pr_err("hv guest-exit result=%d reason=%u vmid=%u run=%u\n",
                       static_cast<int>(result), static_cast<unsigned int>(exit.reason), vm.vmid,
                       vcpu.run_generation);
#endif
        }
        return result;
    }
} // namespace sys::kernel::hypervisor
