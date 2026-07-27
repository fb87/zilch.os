#pragma once

#include <sys/kernel/hypervisor/diagnostics.hh>
#include <sys/kernel/hypervisor/vmid.hh>

namespace sys::kernel::hypervisor
{
    [[nodiscard]] inline error_t reset(virtual_machine_t& vm) noexcept {
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success)
            return vmid_result;
        if (vm.active_vcpus != 0U)
            return error_t::busy;
        for (auto& mapping : vm.mappings)
            mapping = {};
        vm.mapping_count = 0U;
        vm.mapped_pages = 0U;
        vm.state = vm_state::configured;
        audit(vm, audit_action::reset);
        diagnose(vm, 1U, error_t::success);
        return error_t::success;
    }

    [[nodiscard]] inline error_t
    stage2_map(virtual_machine_t& vm, u64 ipa, paddr_t host_address, u64 size, u32 permissions,
               diagnostic_kind diagnostic = diagnostic_kind::unexpected,
               error_t expected = error_t::success, const char* operation = "stage2_map") noexcept {
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success)
            return vmid_result;
        if (!aligned(ipa) || !aligned(host_address) || size == 0U || !aligned(size) ||
            ipa + size < ipa || ipa + size > guest_ipa_limit) {
            diagnose(vm, 2U, error_t::invalid_argument, ipa, size, diagnostic, expected, operation);
            return error_t::invalid_argument;
        }
        if (contains_wx(permissions)) {
            diagnose(vm, 2U, error_t::denied, ipa, permissions, diagnostic, expected, operation);
            return error_t::denied;
        }
        for (const auto& mapping : vm.mappings) {
            if (!mapping.valid)
                continue;
            if (ipa < mapping.ipa + mapping.size && mapping.ipa < ipa + size) {
                diagnose(vm, 2U, error_t::busy, ipa, mapping.ipa, diagnostic, expected, operation);
                return error_t::busy;
            }
        }
        for (auto& mapping : vm.mappings) {
            if (mapping.valid)
                continue;
            mapping = {ipa, host_address, size, permissions, true};
            ++vm.mapping_count;
            const u64 pages = size / page_size;
            if (vm.mapped_pages > ~static_cast<u64>(0U) - pages ||
                vm.map_operations == ~static_cast<u64>(0U)) {
                mapping = {};
                --vm.mapping_count;
                ++vm.accounting_faults;
                return error_t::invalid_argument;
            }
            vm.mapped_pages += pages;
            ++vm.map_operations;
            if (vm.mapped_pages > vm.peak_mapped_pages)
                vm.peak_mapped_pages = vm.mapped_pages;
            vm.state = vm_state::runnable;
            arch::hypervisor::invalidate_stage2(vm.vmid);
            audit(vm, audit_action::map, pages);
            diagnose(vm, 2U, error_t::success, ipa, size, diagnostic, expected, operation);
            return error_t::success;
        }
        diagnose(vm, 2U, error_t::no_memory, ipa, size, diagnostic, expected, operation);
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t stage2_unmap(
        virtual_machine_t& vm, u64 ipa, diagnostic_kind diagnostic = diagnostic_kind::unexpected,
        error_t expected = error_t::success, const char* operation = "stage2_unmap") noexcept {
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success)
            return vmid_result;
        for (auto& mapping : vm.mappings) {
            if (!mapping.valid || mapping.ipa != ipa)
                continue;
            const u64 pages = mapping.size / page_size;
            if (vm.mapping_count == 0U || vm.mapped_pages < pages ||
                vm.unmap_operations == ~static_cast<u64>(0U)) {
                ++vm.accounting_faults;
                return error_t::invalid_argument;
            }
            mapping = {};
            --vm.mapping_count;
            vm.mapped_pages -= pages;
            ++vm.unmap_operations;
            arch::hypervisor::invalidate_stage2(vm.vmid);
            audit(vm, audit_action::unmap, pages);
            diagnose(vm, 3U, error_t::success, ipa, 0U, diagnostic, expected, operation);
            return error_t::success;
        }
        diagnose(vm, 3U, error_t::not_found, ipa, 0U, diagnostic, expected, operation);
        return error_t::not_found;
    }
} // namespace sys::kernel::hypervisor
