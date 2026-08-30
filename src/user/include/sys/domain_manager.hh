#pragma once

#include <sys/vmm.hh>

namespace sys::domain_manager
{
    enum class state : u8 { empty, created, configured, runnable, stopped, faulted };

    struct manager final {
        sys::vmm::machine vm{};
        state lifecycle{state::empty};

        [[nodiscard]] word_t launch(capability_id_t vm_selector, capability_id_t vcpu_selector,
                                    word_t logical_id = 0U, word_t counter_offset = 0U) noexcept {
            return create(vm_selector, vcpu_selector, logical_id, counter_offset);
        }

        [[nodiscard]] word_t create(capability_id_t vm_selector, capability_id_t vcpu_selector,
                                    word_t logical_id = 0U, word_t counter_offset = 0U) noexcept {
            if (lifecycle != state::empty)
                return static_cast<word_t>(error_t::busy);
            const word_t status = vm.create(vm_selector, vcpu_selector, logical_id, counter_offset);
            if (status == static_cast<word_t>(error_t::success))
                lifecycle = state::created;
            return status;
        }

        [[nodiscard]] word_t map_frame(word_t ipa, capability_id_t frame,
                                       word_t permissions) noexcept {
            const word_t status = vm.map_frame(ipa, frame, permissions);
            if (status == static_cast<word_t>(error_t::success))
                lifecycle = state::configured;
            return status;
        }

        [[nodiscard]] word_t configure(word_t pc, word_t pstate, word_t stack) noexcept {
            const word_t status = vm.configure(pc, pstate, stack);
            if (status == static_cast<word_t>(error_t::success))
                lifecycle = state::runnable;
            return status;
        }

        [[nodiscard]] const abi::v1::vm_exit_result& run() noexcept {
            const auto& exit = vm.run();
            if (exit.status != static_cast<word_t>(error_t::success) ||
                exit.reason == abi::v1::vm_exit_reason::unexpected)
                lifecycle = state::faulted;
            return exit;
        }

        [[nodiscard]] word_t pause() noexcept { return sys::vcpu_pause(vm.vcpu); }

        [[nodiscard]] word_t resume() noexcept { return sys::vcpu_resume(vm.vcpu); }

        [[nodiscard]] word_t stop() noexcept { return sys::vcpu_stop(vm.vcpu); }

        [[nodiscard]] word_t destroy() noexcept {
            const word_t status = vm.destroy();
            if (status == static_cast<word_t>(error_t::success))
                lifecycle = state::empty;
            return status;
        }
    };
} // namespace sys::domain_manager
