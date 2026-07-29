#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/hypervisor/stage2.hh>
#include <sys/kernel/hypervisor/vcpu.hh>
#include <sys/kernel/hypervisor/vmid.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/kernel/object/table.hh>

namespace sys::kernel::hypervisor
{
    inline constexpr u32 maximum_dynamic_vms = 4U;
    inline constexpr u32 maximum_dynamic_vcpus = 8U;

    struct dynamic_vm_slot final {
        virtual_machine_t vm{};
        bool in_use{};
    };

    struct dynamic_vcpu_slot final {
        virtual_cpu_t vcpu{};
        bool in_use{};
    };

    inline dynamic_vm_slot dynamic_vms[maximum_dynamic_vms]{};
    inline dynamic_vcpu_slot dynamic_vcpus[maximum_dynamic_vcpus]{};
    inline volatile u32 dynamic_pool_lock{};

    [[nodiscard]] inline u64 next_virtual_timer_deadline(cpu_id_t cpu, u64 fallback) noexcept {
        u64 deadline = fallback;
        for (auto& slot : dynamic_vcpus) {
            if (!__atomic_load_n(&slot.in_use, __ATOMIC_ACQUIRE))
                continue;
            auto& vcpu = slot.vcpu;
            if (vcpu.host_cpu != cpu || !vcpu.timer.armed || vcpu.timer.pending)
                continue;
            auto* header = object::resolve(vcpu.virtual_machine);
            if (header == nullptr || header->type != object::type_t::virtual_machine)
                continue;
            const auto& vm = *reinterpret_cast<const virtual_machine_t*>(header);
            const u64 physical_deadline = vcpu.timer.deadline + vm.counter_offset;
            if (physical_deadline < deadline)
                deadline = physical_deadline;
        }
        return deadline;
    }

    inline void poll_virtual_timers(cpu_id_t cpu, u64 physical_now) noexcept {
        for (auto& slot : dynamic_vcpus) {
            if (!__atomic_load_n(&slot.in_use, __ATOMIC_ACQUIRE))
                continue;
            auto& vcpu = slot.vcpu;
            if (vcpu.host_cpu != cpu || __atomic_load_n(&vcpu.running, __ATOMIC_ACQUIRE))
                continue;
            auto* header = object::resolve(vcpu.virtual_machine);
            if (header == nullptr || header->type != object::type_t::virtual_machine)
                continue;
            auto& vm = *reinterpret_cast<virtual_machine_t*>(header);
            lock_vm(vm);
            if (!vcpu.running &&
                vcpu.timer.expire(virtual_counter(physical_now, vm.counter_offset))) {
                const error_t status = vcpu.interrupt_state.inject(27U);
                if (status == error_t::success || status == error_t::busy) {
                    vcpu.virtual_irq = 27U;
                    __atomic_store_n(&vcpu.virtual_irq_pending, true, __ATOMIC_RELEASE);
                    if (vcpu.lifecycle == vcpu_state::blocked)
                        vcpu.lifecycle = vcpu_state::runnable;
                }
            }
            unlock_vm(vm);
        }
    }

    inline void lock_dynamic_pool() noexcept {
        while (__atomic_exchange_n(&dynamic_pool_lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
        lock_order::acquired(lock_order::rank::hypervisor_pool, &dynamic_pool_lock);
    }

    inline void unlock_dynamic_pool() noexcept {
        lock_order::released(lock_order::rank::hypervisor_pool, &dynamic_pool_lock);
        __atomic_store_n(&dynamic_pool_lock, 0U, __ATOMIC_RELEASE);
    }

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

    [[nodiscard]] inline error_t create_vm(u64 counter_offset,
                                           virtual_machine_t*& result) noexcept {
        result = nullptr;
        lock_dynamic_pool();
        for (u32 index = 0U; index < maximum_dynamic_vms; ++index) {
            dynamic_vm_slot& slot = dynamic_vms[index];
            if (slot.in_use)
                continue;
            slot.in_use = true;
            scrub_bytes(&slot.vm, sizeof(slot.vm));
            slot.vm.id = index + 1U;
            slot.vm.counter_offset = counter_offset;
            slot.vm.stage2_table_capacity = maximum_stage2_table_pages;
            error_t status = memory::allocate_physical_page(slot.vm.stage_2_root);
            if (status == error_t::success) {
                slot.vm.stage2_table_addresses[0] = slot.vm.stage_2_root;
                slot.vm.stage2_table_pages = 1U;
                status = allocate_vmid(slot.vm.vmid, slot.vm.vmid_generation);
            }
            if (status == error_t::success)
                status = object::register_dynamic_object(slot.vm.object,
                                                         object::type_t::virtual_machine);
            if (status != error_t::success) {
                if (slot.vm.vmid != 0U)
                    (void)release_vmid(slot.vm.vmid, slot.vm.vmid_generation);
                if (slot.vm.stage_2_root != 0U)
                    (void)memory::release_physical_page(slot.vm.stage_2_root);
                scrub_bytes(&slot.vm, sizeof(slot.vm));
                slot.in_use = false;
                unlock_dynamic_pool();
                return status;
            }
            slot.vm.state = vm_state::configured;
            result = &slot.vm;
            unlock_dynamic_pool();
            return error_t::success;
        }
        unlock_dynamic_pool();
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t create_vcpu(virtual_machine_t& vm, u32 logical_id,
                                             virtual_cpu_t*& result) noexcept {
        result = nullptr;
        lock_dynamic_pool();
        lock_vm(vm);
        if (vm.state == vm_state::inactive || vm.vcpu_count >= maximum_vcpus_per_vm) {
            unlock_vm(vm);
            unlock_dynamic_pool();
            return error_t::invalid_argument;
        }
        for (u32 index = 0U; index < maximum_dynamic_vcpus; ++index) {
            dynamic_vcpu_slot& slot = dynamic_vcpus[index];
            if (slot.in_use)
                continue;
            slot.in_use = true;
            scrub_bytes(&slot.vcpu, sizeof(slot.vcpu));
            slot.vcpu.id = index + 1U;
            slot.vcpu.logical_id = logical_id;
            slot.vcpu.virtual_machine = object::reference(vm.object);
            slot.vcpu.state = vm_state::configured;
            slot.vcpu.lifecycle = vcpu_state::configured;
            const error_t status =
                object::register_dynamic_object(slot.vcpu.object, object::type_t::virtual_cpu);
            if (status != error_t::success) {
                scrub_bytes(&slot.vcpu, sizeof(slot.vcpu));
                slot.in_use = false;
                unlock_vm(vm);
                unlock_dynamic_pool();
                return status;
            }
            ++vm.vcpu_count;
            result = &slot.vcpu;
            unlock_vm(vm);
            unlock_dynamic_pool();
            return error_t::success;
        }
        unlock_vm(vm);
        unlock_dynamic_pool();
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t destroy_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.object.id < object::dynamic_id_base)
            return error_t::busy;
        auto* vm_header = object::resolve(vcpu.virtual_machine);
        if (vm_header == nullptr || vm_header->type != object::type_t::virtual_machine)
            return error_t::not_found;
        auto& vm = *reinterpret_cast<virtual_machine_t*>(vm_header);
        lock_vm(vm);
        if (vcpu.running) {
            unlock_vm(vm);
            return error_t::busy;
        }
        const object::reference_t reference = object::reference(vcpu.object);
        (void)capability::revoke_reference(reference);
        const error_t status = object::unregister_object(reference);
        if (status != error_t::success) {
            unlock_vm(vm);
            return status;
        }
        if (vm.vcpu_count != 0U)
            --vm.vcpu_count;
        unlock_vm(vm);
        lock_dynamic_pool();
        for (auto& slot : dynamic_vcpus) {
            if (&slot.vcpu != &vcpu)
                continue;
            scrub_vcpu_state(slot.vcpu);
            scrub_bytes(&slot.vcpu, sizeof(slot.vcpu));
            slot.in_use = false;
            break;
        }
        unlock_dynamic_pool();
        return error_t::success;
    }

    [[nodiscard]] inline error_t destroy_vm(virtual_machine_t& vm) noexcept {
        if (vm.object.id < object::dynamic_id_base)
            return error_t::busy;
        lock_vm(vm);
        if (vm.active_vcpus != 0U || vm.vcpu_count != 0U || vm.mapping_count != 0U) {
            unlock_vm(vm);
            return error_t::busy;
        }
        const u16 vmid = vm.vmid;
        const u32 generation = vm.vmid_generation;
        const object::reference_t reference = object::reference(vm.object);
        (void)capability::revoke_reference(reference);
        const error_t status = object::unregister_object(reference);
        if (status != error_t::success) {
            unlock_vm(vm);
            return status;
        }
        const error_t released = release_vmid(vmid, generation);
        for (u32 page = 0U; page < vm.stage2_table_pages; ++page) {
            if (vm.stage2_table_addresses[page] != 0U)
                (void)memory::release_physical_page(vm.stage2_table_addresses[page]);
        }
        unlock_vm(vm);
        lock_dynamic_pool();
        for (auto& slot : dynamic_vms) {
            if (&slot.vm != &vm)
                continue;
            scrub_bytes(&slot.vm, sizeof(slot.vm));
            slot.in_use = false;
            break;
        }
        unlock_dynamic_pool();
        return released;
    }

    [[nodiscard]] inline error_t initialize() noexcept {
        if constexpr (!arch::hypervisor::active)
            return error_t::unsupported;
        bootstrap_vm.id = 0U;
        bootstrap_vm.counter_offset = 0U;
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
        vcpu.timer.reset();
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
        vm.counter_offset = 0U;
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
