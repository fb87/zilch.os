#pragma once

#include <sys/hypervisor.hh>
#include <sys/types.hh>

namespace sys::hypervisor_lifecycle_certification
{
    inline constexpr word_t complete_mask = 0xffU;

    [[nodiscard]] inline word_t run() noexcept {
        constexpr capability_id_t vm = 56U;
        constexpr capability_id_t vcpu = 57U;
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t busy = static_cast<word_t>(error_t::busy);
        const word_t not_found = static_cast<word_t>(error_t::not_found);
        const word_t denied = static_cast<word_t>(error_t::denied);
        word_t mask = 0U;

        if (vm_create(vm, 0x1000U) == success)
            mask |= 1U << 0U;
        if (vcpu_create(vm, vcpu, 0U) == success)
            mask |= 1U << 1U;
        if (vm_destroy(vm) == busy)
            mask |= 1U << 2U;
        if (vcpu_destroy(vcpu) == success)
            mask |= 1U << 3U;
        const word_t stale_vcpu = vcpu_stop(vcpu);
        if (stale_vcpu == not_found || stale_vcpu == denied)
            mask |= 1U << 4U;
        if (vm_destroy(vm) == success)
            mask |= 1U << 5U;
        const word_t stale_vm = hypervisor_invoke(abi::v1::hypervisor_operation::vm_reset, vm);
        if (stale_vm == not_found || stale_vm == denied)
            mask |= 1U << 6U;
        if (vm_create(vm, 0x2000U) == success && vm_destroy(vm) == success)
            mask |= 1U << 7U;
        return mask;
    }
} // namespace sys::hypervisor_lifecycle_certification
