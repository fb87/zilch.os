#pragma once

#include <sys/kernel/hypervisor/object.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::hypervisor
{
    [[nodiscard]] inline constexpr bool fatal_guest_exit(abi::v1::vm_exit_reason reason) noexcept {
        return reason == abi::v1::vm_exit_reason::unexpected;
    }

    [[nodiscard]] inline error_t configure_vcpu(virtual_cpu_t& vcpu, u64 pc, u64 pstate,
                                                u64 sp) noexcept {
        if (!aligned(pc) || !aligned(sp))
            return error_t::invalid_argument;
        vcpu.context.pc = pc;
        vcpu.context.pstate = pstate;
        vcpu.context.sp_el1 = sp;
        vcpu.interrupt_state.reset();
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

    [[nodiscard]] inline error_t run(virtual_cpu_t& vcpu, exit_record& exit) noexcept {
        auto* vm_header = object::resolve(vcpu.virtual_machine);
        if (vm_header == nullptr || vm_header->type != object::type_t::virtual_machine)
            return error_t::not_found;
        auto& vm = *reinterpret_cast<virtual_machine_t*>(vm_header);
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success)
            return vmid_result;
        if (vcpu.state != vm_state::runnable || vm.mapping_count == 0U)
            return error_t::invalid_argument;
        if (__atomic_exchange_n(&vcpu.running, true, __ATOMIC_ACQ_REL))
            return error_t::busy;
        if (vm.run_entries == ~static_cast<u64>(0U)) {
            ++vm.accounting_faults;
            __atomic_store_n(&vcpu.running, false, __ATOMIC_RELEASE);
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
        const bool pending_irq = __atomic_load_n(&vcpu.virtual_irq_pending, __ATOMIC_ACQUIRE);
        const error_t result =
            arch::hypervisor::run_guest(vm.vmid, vm.stage_2_root, vcpu.context, exit, pending_irq);
        if (pending_irq && result == error_t::success) {
            __atomic_store_n(&vcpu.virtual_irq_pending, false, __ATOMIC_RELEASE);
            vcpu.interrupt_state.reset();
        }
        vcpu.last_exit = exit;
        emergency::trace(emergency::event::vm_exit, vcpu.id, static_cast<u64>(exit.reason),
                         exit.syndrome, exit.qualification, exit.guest_pc);
        --vm.active_vcpus;
        if (vm.run_exits == ~static_cast<u64>(0U)) {
            ++vm.accounting_faults;
        } else {
            ++vm.run_exits;
        }
        audit(vm, audit_action::run_exit, vcpu.id);
        __atomic_store_n(&vcpu.running, false, __ATOMIC_RELEASE);
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
#if CONFIG_VERBOSE_DIAGNOSTICS
            pr_err("hv guest-exit result=%d reason=%u esr=%llx far=%llx ipa=%llx pc=%llx "
                   "pstate=%llx vmid=%u run=%u\n",
                   static_cast<int>(result), static_cast<unsigned int>(exit.reason),
                   static_cast<unsigned long long>(exit.syndrome),
                   static_cast<unsigned long long>(exit.fault_address),
                   static_cast<unsigned long long>(exit.qualification),
                   static_cast<unsigned long long>(exit.guest_pc),
                   static_cast<unsigned long long>(vcpu.context.pstate), vm.vmid,
                   vcpu.run_generation);
            pr_err("hv guest-regs x0=%llx x1=%llx x2=%llx x3=%llx x4=%llx x5=%llx x6=%llx x7=%llx "
                   "sp1=%llx\n",
                   static_cast<unsigned long long>(vcpu.context.x[0]),
                   static_cast<unsigned long long>(vcpu.context.x[1]),
                   static_cast<unsigned long long>(vcpu.context.x[2]),
                   static_cast<unsigned long long>(vcpu.context.x[3]),
                   static_cast<unsigned long long>(vcpu.context.x[4]),
                   static_cast<unsigned long long>(vcpu.context.x[5]),
                   static_cast<unsigned long long>(vcpu.context.x[6]),
                   static_cast<unsigned long long>(vcpu.context.x[7]),
                   static_cast<unsigned long long>(vcpu.context.sp_el1));
#else
            pr_err("hv guest-exit result=%d reason=%u vmid=%u run=%u\n", static_cast<int>(result),
                   static_cast<unsigned int>(exit.reason), vm.vmid, vcpu.run_generation);
#endif
        }
        return result;
    }
} // namespace sys::kernel::hypervisor
