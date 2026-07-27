#pragma once

#include <sys/kernel/hypervisor/stage2.hh>
#include <sys/kernel/hypervisor/vcpu.hh>
#include <sys/kernel/hypervisor/vmid.hh>
#include <sys/kernel/object/table.hh>

namespace sys::kernel::hypervisor
{
    inline void scrub_bytes(void* address, usize_t size) noexcept {
        auto* bytes = reinterpret_cast<volatile u8*>(address);
        for (usize_t index = 0U; index < size; ++index)
            bytes[index] = 0U;
    }

    inline void scrub_vcpu_state(virtual_cpu_t& vcpu) noexcept {
        vcpu.interrupt_state.reset();
        vcpu.timer.reset();
        vcpu.virtual_irq_pending = false;
        vcpu.virtual_irq = 0U;
        vcpu.previous_host_cpu = 0U;
        vcpu.migration_count = 0U;
        vcpu.run_generation = 0U;
        vcpu.executed_quanta = 0U;
        vcpu.virtual_ipi_count = 0U;
        scrub_bytes(&vcpu.context, sizeof(vcpu.context));
        scrub_bytes(&vcpu.last_exit, sizeof(vcpu.last_exit));
        scrub_bytes(&vcpu.last_diagnostic, sizeof(vcpu.last_diagnostic));
    }

    [[nodiscard]] inline error_t initialize() noexcept {
        if constexpr (!arch::hypervisor::active)
            return error_t::unsupported;
        bootstrap_vm.id = 0U;
        error_t vmid_result = allocate_vmid(bootstrap_vm.vmid, bootstrap_vm.vmid_generation);
        if (vmid_result != error_t::success)
            return vmid_result;
        bootstrap_vm.state = vm_state::configured;
        error_t result = object::register_object(bootstrap_vm.object, vm_object_id,
                                                 object::type_t::virtual_machine);
        if (result != error_t::success)
            return result;
        bootstrap_vcpu.id = 0U;
        bootstrap_vcpu.virtual_machine = object::reference(bootstrap_vm.object);
        bootstrap_vcpu.state = vm_state::configured;
        bootstrap_vcpu.lifecycle = vcpu_state::configured;
        result = object::register_object(bootstrap_vcpu.object, vcpu_object_id,
                                         object::type_t::virtual_cpu);
        if (result != error_t::success)
            return result;
        return arch::hypervisor::configure_host();
    }

    [[nodiscard]] inline error_t pause_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.running)
            return error_t::busy;
        if (vcpu.lifecycle != vcpu_state::runnable && vcpu.lifecycle != vcpu_state::blocked)
            return error_t::invalid_argument;
        vcpu.lifecycle = vcpu_state::paused;
        if (auto* header = object::resolve(vcpu.virtual_machine); header != nullptr)
            audit(*reinterpret_cast<virtual_machine_t*>(header), audit_action::pause, vcpu.id);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resume_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.lifecycle != vcpu_state::paused)
            return error_t::invalid_argument;
        vcpu.lifecycle = vcpu_state::runnable;
        if (auto* header = object::resolve(vcpu.virtual_machine); header != nullptr)
            audit(*reinterpret_cast<virtual_machine_t*>(header), audit_action::resume, vcpu.id);
        return error_t::success;
    }

    [[nodiscard]] inline error_t stop_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.running)
            return error_t::busy;
        vcpu.lifecycle = vcpu_state::stopped;
        vcpu.state = vm_state::stopped;
        vcpu.interrupt_state.reset();
        vcpu.virtual_irq_pending = false;
        if (auto* header = object::resolve(vcpu.virtual_machine); header != nullptr)
            audit(*reinterpret_cast<virtual_machine_t*>(header), audit_action::stop, vcpu.id);
        return error_t::success;
    }

    [[nodiscard]] inline error_t teardown_vm(virtual_machine_t& vm, virtual_cpu_t* vcpus,
                                             u32 count) noexcept {
        if (count > maximum_vcpus_per_vm)
            return error_t::invalid_argument;
        for (u32 index = 0U; index < count; ++index) {
            if (vcpus[index].running)
                return error_t::busy;
        }
        vm.state = vm_state::stopped;
        for (u32 index = 0U; index < count; ++index) {
            const error_t result = stop_vcpu(vcpus[index]);
            if (result != error_t::success)
                return result;
            scrub_vcpu_state(vcpus[index]);
        }
        for (auto& mapping : vm.mappings)
            mapping = {};
        vm.mapping_count = 0U;
        vm.mapped_pages = 0U;
        vm.active_vcpus = 0U;
        const u16 old_vmid = vm.vmid;
        const u32 old_vmid_generation = vm.vmid_generation;
        vm.vmid = 0U;
        vm.vmid_generation = 0U;
        vm.stage_2_root = 0U;
        vm.state = vm_state::inactive;
        audit(vm, audit_action::teardown, count);
        return release_vmid(old_vmid, old_vmid_generation);
    }

    [[nodiscard]] inline bool mappings_isolated(const virtual_machine_t& first,
                                                const virtual_machine_t& second) noexcept {
        if (first.vmid == 0U || second.vmid == 0U || first.vmid == second.vmid)
            return false;
        if (first.stage_2_root == second.stage_2_root)
            return false;
        for (const auto& left : first.mappings) {
            if (!left.valid)
                continue;
            for (const auto& right : second.mappings) {
                if (!right.valid)
                    continue;
                const bool host_overlap = left.host_address < right.host_address + right.size &&
                                          right.host_address < left.host_address + left.size;
                if (host_overlap)
                    return false;
            }
        }
        return true;
    }
} // namespace sys::kernel::hypervisor
