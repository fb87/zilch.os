#pragma once

#include <sys/hypervisor.hh>

namespace sys::vmm
{
    enum class state : u8 { empty, created, configured, runnable, stopped, faulted };

    struct machine final {
        capability_id_t vm{};
        capability_id_t vcpu{};
        state lifecycle{state::empty};
        abi::v1::vm_exit_result last_exit{};

        [[nodiscard]] word_t create(capability_id_t vm_selector, capability_id_t vcpu_selector,
                                    word_t logical_id = 0U, word_t counter_offset = 0U) noexcept {
            if (lifecycle != state::empty)
                return static_cast<word_t>(error_t::busy);
            word_t status = vm_create(vm_selector, counter_offset);
            if (status != static_cast<word_t>(error_t::success))
                return status;
            status = vcpu_create(vm_selector, vcpu_selector, logical_id);
            if (status != static_cast<word_t>(error_t::success)) {
                (void)vm_destroy(vm_selector);
                return status;
            }
            vm = vm_selector;
            vcpu = vcpu_selector;
            lifecycle = state::created;
            return status;
        }

        [[nodiscard]] word_t map_frame(word_t ipa, capability_id_t frame,
                                       word_t permissions) noexcept {
            const word_t status = hypervisor_invoke(abi::v1::hypervisor_operation::stage2_map, vm,
                                                    ipa, frame, permissions);
            if (status == static_cast<word_t>(error_t::success))
                lifecycle = state::configured;
            return status;
        }

        [[nodiscard]] word_t configure(word_t pc, word_t pstate, word_t stack) noexcept {
            const word_t status = hypervisor_invoke(abi::v1::hypervisor_operation::vcpu_configure,
                                                    vcpu, pc, pstate, stack);
            if (status == static_cast<word_t>(error_t::success))
                lifecycle = state::runnable;
            return status;
        }

        [[nodiscard]] const abi::v1::vm_exit_result& run() noexcept {
            last_exit = vcpu_run(vcpu);
            if (last_exit.status != static_cast<word_t>(error_t::success) ||
                last_exit.reason == abi::v1::vm_exit_reason::unexpected)
                lifecycle = state::faulted;
            return last_exit;
        }

        [[nodiscard]] word_t inject(u16 irq) noexcept {
            return hypervisor_invoke(abi::v1::hypervisor_operation::virtual_irq_inject, vcpu, irq);
        }

        [[nodiscard]] word_t unmap(word_t ipa) noexcept {
            return hypervisor_invoke(abi::v1::hypervisor_operation::stage2_unmap, vm, ipa);
        }

        [[nodiscard]] word_t destroy() noexcept {
            if (lifecycle == state::empty)
                return static_cast<word_t>(error_t::not_found);
            (void)vcpu_stop(vcpu);
            word_t status = vcpu_destroy(vcpu);
            if (status != static_cast<word_t>(error_t::success))
                return status;
            status = vm_destroy(vm);
            if (status == static_cast<word_t>(error_t::success)) {
                vm = 0U;
                vcpu = 0U;
                lifecycle = state::empty;
            }
            return status;
        }
    };
} // namespace sys::vmm
