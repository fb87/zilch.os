#pragma once

#include <sys/kernel/hypervisor/lifecycle.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::hypervisor_lifecycle
{
    /*
     * VM and vCPU lifecycle guards.
     *
     * The integration suites create real VMs and run real guests; what they
     * do not do is drive the refusal paths, because a healthy sequence never
     * reaches them. Those refusals are the entire safety story of teardown:
     * they are what stops a VM being dismantled out from under a vCPU that
     * is executing, or its stage-2 tables being freed while mappings still
     * point into them.
     *
     * Every case below returns before touching the object table or the
     * dynamic pool, which is what makes them safe to drive against plain
     * structs rather than a live VM. Anything that proceeds past the guard
     * belongs in the integration suites, not here.
     */
    [[nodiscard]] inline error_t run() noexcept
    {
        using namespace sys::kernel::hypervisor;
        // Statics, never whole-struct assigned: these carry arrays, so `vm = {}`
        // emits a memcpy/memset the freestanding kernel does not link. Fields are
        // reset individually below, and vm.lock is deliberately never touched --
        // see LOCKING_PROTOCOL.md on why a lock word must not be rewound.
        static virtual_machine_t vm{};
        static virtual_cpu_t vcpu{};
        const auto clear_vm = [&]() noexcept {
            vm.active_vcpus = 0U;
            vm.vcpu_count = 0U;
            vm.mapping_count = 0U;
        };

        /*
         * Statically allocated objects are not destroyable at all. The
         * bounded pools are the product contract; a destroy that scrubbed
         * a static slot would hand the next caller a recycled object the
         * generation check could not protect them from.
         */
        clear_vm();
        vm.object.id = object::dynamic_id_base - 1U;
        if (destroy_vm(vm) != error_t::busy)
            return error_t::invalid_argument;
        vcpu.running = false;
        vcpu.object.id = object::dynamic_id_base - 1U;
        if (destroy_vcpu(vcpu) != error_t::busy)
            return error_t::invalid_argument;

        // Each child-accounting guard, independently: any one of them alone
        // must refuse, so none is load-bearing only in combination.
        clear_vm();
        vm.object.id = object::dynamic_id_base;
        vm.active_vcpus = 1U;
        if (destroy_vm(vm) != error_t::busy)
            return error_t::invalid_argument; // a vCPU is executing

        clear_vm();
        vm.vcpu_count = 1U;
        if (destroy_vm(vm) != error_t::busy)
            return error_t::invalid_argument; // a vCPU still exists

        clear_vm();
        vm.mapping_count = 1U;
        if (destroy_vm(vm) != error_t::busy)
            return error_t::invalid_argument; // stage-2 mappings still live

        /*
         * vCPU state machine. pause is legal only from runnable or blocked,
         * resume only from paused, and neither is legal while the vCPU is
         * actually executing -- a pause that took effect mid-execution would
         * be a lie about a context still in guest registers.
         */
        vcpu.object.id = object::dynamic_id_base;
        vcpu.lifecycle = vcpu_state::runnable;
        vcpu.running = true;
        if (pause_vcpu(vcpu) != error_t::busy || stop_vcpu(vcpu) != error_t::busy)
            return error_t::invalid_argument;

        vcpu.running = false;
        if (pause_vcpu(vcpu) != error_t::success || vcpu.lifecycle != vcpu_state::paused)
            return error_t::invalid_argument;
        if (pause_vcpu(vcpu) != error_t::invalid_argument)
            return error_t::invalid_argument; // already paused is not runnable/blocked
        if (resume_vcpu(vcpu) != error_t::success || vcpu.lifecycle != vcpu_state::runnable)
            return error_t::invalid_argument;
        if (resume_vcpu(vcpu) != error_t::invalid_argument)
            return error_t::invalid_argument; // resume is only legal from paused

        /*
         * stop must leave nothing armed behind it. A stopped vCPU that kept
         * a pending virtual interrupt or a live timer deadline would deliver
         * into a context that is no longer running.
         */
        vcpu.virtual_irq_pending = true;
        if (stop_vcpu(vcpu) != error_t::success)
            return error_t::invalid_argument;
        if (vcpu.lifecycle != vcpu_state::stopped || vcpu.state != vm_state::stopped ||
            vcpu.virtual_irq_pending)
            return error_t::invalid_argument;

        pr_info("[TEST] name=vm_lifecycle_guards result=PASS static_undestroyable=1 "
                "active_vcpus=1 vcpu_count=1 mapping_count=1 running_refuses=1 "
                "state_machine=1 stop_disarms=1\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::hypervisor_lifecycle
