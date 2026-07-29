#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/hypervisor/diagnostics.hh>
#include <sys/kernel/hypervisor/vmid.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/kernel/memory/manager.hh>

namespace sys::kernel::hypervisor
{
    inline constexpr u64 stage2_descriptor_valid = 1ULL;
    inline constexpr u64 stage2_descriptor_table = 3ULL;
    inline constexpr u64 stage2_descriptor_access = 1ULL << 10U;
    inline constexpr u64 stage2_descriptor_inner_shareable = 3ULL << 8U;
    inline constexpr u64 stage2_descriptor_read_only = 1ULL << 6U;
    inline constexpr u64 stage2_descriptor_read_write = 3ULL << 6U;
    inline constexpr u64 stage2_descriptor_execute_never = 3ULL << 53U;
    inline constexpr u64 stage2_descriptor_normal_memory = 0xfULL << 2U;
    inline constexpr u64 stage2_address_mask = 0x0000fffffffff000ULL;

    inline void lock_vm(virtual_machine_t& vm) noexcept {
        while (__atomic_exchange_n(&vm.lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
        lock_order::acquired(lock_order::rank::hypervisor_object, &vm.lock);
    }

    inline void unlock_vm(virtual_machine_t& vm) noexcept {
        lock_order::released(lock_order::rank::hypervisor_object, &vm.lock);
        __atomic_store_n(&vm.lock, 0U, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline u64* stage2_table(virtual_machine_t& vm, u32 page) noexcept {
        if (page >= vm.stage2_table_capacity || vm.stage2_table_addresses[page] == 0U)
            return nullptr;
        return reinterpret_cast<u64*>(vm.stage2_table_addresses[page]);
    }

    [[nodiscard]] inline u64* allocate_stage2_table(virtual_machine_t& vm) noexcept {
        if (vm.stage2_table_pages >= vm.stage2_table_capacity)
            return nullptr;
        paddr_t address{};
        if (memory::allocate_physical_page(address) != error_t::success)
            return nullptr;
        vm.stage2_table_addresses[vm.stage2_table_pages++] = address;
        return reinterpret_cast<u64*>(address);
    }

    [[nodiscard]] inline u64 stage2_leaf_descriptor(paddr_t address, u32 permissions) noexcept {
        u64 descriptor =
            (address & stage2_address_mask) | stage2_descriptor_table | stage2_descriptor_access;
        descriptor |= (permissions & static_cast<u32>(stage2_permission::write)) != 0U
                          ? stage2_descriptor_read_write
                          : stage2_descriptor_read_only;
        if ((permissions & static_cast<u32>(stage2_permission::device)) == 0U)
            descriptor |= stage2_descriptor_normal_memory | stage2_descriptor_inner_shareable;
        if ((permissions & static_cast<u32>(stage2_permission::execute)) == 0U)
            descriptor |= stage2_descriptor_execute_never;
        return descriptor;
    }

    [[nodiscard]] inline error_t install_stage2_page(virtual_machine_t& vm, u64 ipa,
                                                     paddr_t host_address,
                                                     u32 permissions) noexcept {
        u64* root = stage2_table(vm, 0U);
        if (root == nullptr)
            return error_t::invalid_argument;
        const u32 l1_index = static_cast<u32>((ipa >> 30U) & 0x3U);
        const u32 l2_index = static_cast<u32>((ipa >> 21U) & 0x1ffU);
        const u32 l3_index = static_cast<u32>((ipa >> 12U) & 0x1ffU);
        u64* l2 = nullptr;
        if ((root[l1_index] & stage2_descriptor_valid) == 0U) {
            l2 = allocate_stage2_table(vm);
            if (l2 == nullptr)
                return error_t::no_memory;
            root[l1_index] =
                (reinterpret_cast<paddr_t>(l2) & stage2_address_mask) | stage2_descriptor_table;
        } else {
            l2 = reinterpret_cast<u64*>(root[l1_index] & stage2_address_mask);
        }
        u64* l3 = nullptr;
        if ((l2[l2_index] & stage2_descriptor_valid) == 0U) {
            l3 = allocate_stage2_table(vm);
            if (l3 == nullptr)
                return error_t::no_memory;
            l2[l2_index] =
                (reinterpret_cast<paddr_t>(l3) & stage2_address_mask) | stage2_descriptor_table;
        } else {
            l3 = reinterpret_cast<u64*>(l2[l2_index] & stage2_address_mask);
        }
        if ((l3[l3_index] & stage2_descriptor_valid) != 0U)
            return error_t::busy;
        l3[l3_index] = stage2_leaf_descriptor(host_address, permissions);
        return error_t::success;
    }

    [[nodiscard]] inline error_t rebuild_stage2(virtual_machine_t& vm) noexcept {
        if (vm.stage2_table_capacity == 0U)
            return error_t::success; // modeled/bootstrap objects may own external tables
        for (u32 page = 1U; page < vm.stage2_table_pages; ++page) {
            if (vm.stage2_table_addresses[page] != 0U)
                (void)memory::release_physical_page(vm.stage2_table_addresses[page]);
            vm.stage2_table_addresses[page] = 0U;
        }
        vm.stage2_table_pages = 1U;
        u64* root = stage2_table(vm, 0U);
        if (root == nullptr)
            return error_t::invalid_argument;
        for (u32 entry = 0U; entry < stage2_entries_per_table; ++entry)
            root[entry] = 0U;
        for (const auto& mapping : vm.mappings) {
            if (!mapping.valid)
                continue;
            for (u64 offset = 0U; offset < mapping.size; offset += page_size) {
                const error_t status = install_stage2_page(
                    vm, mapping.ipa + offset, mapping.host_address + offset, mapping.permissions);
                if (status != error_t::success)
                    return status;
            }
        }
        __asm__ volatile("dsb ishst" ::: "memory");
        return error_t::success;
    }

    [[nodiscard]] inline constexpr bool valid_stage2_permissions(u32 permissions) noexcept {
        constexpr u32 read = static_cast<u32>(stage2_permission::read);
        constexpr u32 write = static_cast<u32>(stage2_permission::write);
        constexpr u32 execute = static_cast<u32>(stage2_permission::execute);
        constexpr u32 device = static_cast<u32>(stage2_permission::device);
        constexpr u32 allowed = read | write | execute | device;
        return (permissions & ~allowed) == 0U && (permissions & read) != 0U &&
               !contains_wx(permissions) &&
               ((permissions & device) == 0U || (permissions & execute) == 0U);
    }

    [[nodiscard]] inline error_t reset(virtual_machine_t& vm) noexcept {
        lock_vm(vm);
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success) {
            unlock_vm(vm);
            return vmid_result;
        }
        if (vm.active_vcpus != 0U) {
            unlock_vm(vm);
            return error_t::busy;
        }
        for (auto& mapping : vm.mappings)
            mapping = {};
        vm.mapping_count = 0U;
        vm.mapped_pages = 0U;
        (void)rebuild_stage2(vm);
        vm.state = vm_state::configured;
        audit(vm, audit_action::reset);
        diagnose(vm, 1U, error_t::success);
        unlock_vm(vm);
        return error_t::success;
    }

    [[nodiscard]] inline error_t
    stage2_map(virtual_machine_t& vm, u64 ipa, paddr_t host_address, u64 size, u32 permissions,
               diagnostic_kind diagnostic = diagnostic_kind::unexpected,
               error_t expected = error_t::success, const char* operation = "stage2_map") noexcept {
        lock_vm(vm);
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success) {
            unlock_vm(vm);
            return vmid_result;
        }
        if (!aligned(ipa) || !aligned(host_address) || size == 0U || !aligned(size) ||
            ipa + size < ipa || ipa + size > guest_ipa_limit) {
            diagnose(vm, 2U, error_t::invalid_argument, ipa, size, diagnostic, expected, operation);
            unlock_vm(vm);
            return error_t::invalid_argument;
        }
        if (!valid_stage2_permissions(permissions)) {
            diagnose(vm, 2U, error_t::denied, ipa, permissions, diagnostic, expected, operation);
            unlock_vm(vm);
            return error_t::denied;
        }
        for (const auto& mapping : vm.mappings) {
            if (!mapping.valid)
                continue;
            if (ipa < mapping.ipa + mapping.size && mapping.ipa < ipa + size) {
                diagnose(vm, 2U, error_t::busy, ipa, mapping.ipa, diagnostic, expected, operation);
                unlock_vm(vm);
                return error_t::busy;
            }
        }
        for (auto& mapping : vm.mappings) {
            if (mapping.valid)
                continue;
            mapping = {ipa,
                       host_address,
                       size,
                       permissions,
                       true,
                       true,
                       (permissions & static_cast<u32>(stage2_permission::write)) != 0U,
                       1U};
            ++vm.mapping_count;
            const u64 pages = size / page_size;
            if (vm.mapped_pages > ~static_cast<u64>(0U) - pages ||
                vm.map_operations == ~static_cast<u64>(0U)) {
                mapping = {};
                --vm.mapping_count;
                ++vm.accounting_faults;
                unlock_vm(vm);
                return error_t::invalid_argument;
            }
            const error_t table_result = rebuild_stage2(vm);
            if (table_result != error_t::success) {
                mapping = {};
                --vm.mapping_count;
                vm.mapped_pages -= pages;
                --vm.map_operations;
                (void)rebuild_stage2(vm);
                unlock_vm(vm);
                return table_result;
            }
            vm.mapped_pages += pages;
            ++vm.map_operations;
            if (vm.mapped_pages > vm.peak_mapped_pages)
                vm.peak_mapped_pages = vm.mapped_pages;
            vm.state = vm_state::runnable;
            arch::hypervisor::invalidate_stage2(vm.vmid);
            audit(vm, audit_action::map, pages);
            diagnose(vm, 2U, error_t::success, ipa, size, diagnostic, expected, operation);
            unlock_vm(vm);
            return error_t::success;
        }
        diagnose(vm, 2U, error_t::no_memory, ipa, size, diagnostic, expected, operation);
        unlock_vm(vm);
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t stage2_unmap(
        virtual_machine_t& vm, u64 ipa, diagnostic_kind diagnostic = diagnostic_kind::unexpected,
        error_t expected = error_t::success, const char* operation = "stage2_unmap") noexcept {
        lock_vm(vm);
        const error_t vmid_result = ensure_vmid(vm);
        if (vmid_result != error_t::success) {
            unlock_vm(vm);
            return vmid_result;
        }
        if (vm.active_vcpus != 0U) {
            unlock_vm(vm);
            return error_t::busy;
        }
        for (auto& mapping : vm.mappings) {
            if (!mapping.valid || mapping.ipa != ipa)
                continue;
            const u64 pages = mapping.size / page_size;
            if (vm.mapping_count == 0U || vm.mapped_pages < pages ||
                vm.unmap_operations == ~static_cast<u64>(0U)) {
                ++vm.accounting_faults;
                unlock_vm(vm);
                return error_t::invalid_argument;
            }
            mapping = {};
            --vm.mapping_count;
            vm.mapped_pages -= pages;
            ++vm.unmap_operations;
            const error_t table_result = rebuild_stage2(vm);
            if (table_result != error_t::success) {
                ++vm.accounting_faults;
                unlock_vm(vm);
                return table_result;
            }
            arch::hypervisor::invalidate_stage2(vm.vmid);
            audit(vm, audit_action::unmap, pages);
            diagnose(vm, 3U, error_t::success, ipa, 0U, diagnostic, expected, operation);
            unlock_vm(vm);
            return error_t::success;
        }
        diagnose(vm, 3U, error_t::not_found, ipa, 0U, diagnostic, expected, operation);
        unlock_vm(vm);
        return error_t::not_found;
    }

    [[nodiscard]] inline error_t stage2_tracking(virtual_machine_t& vm, u64 ipa, bool clear,
                                                 u64& flags) noexcept {
        flags = 0U;
        lock_vm(vm);
        for (auto& mapping : vm.mappings) {
            if (!mapping.valid || ipa < mapping.ipa || ipa >= mapping.ipa + mapping.size)
                continue;
            flags = (mapping.accessed ? 1U : 0U) | (mapping.dirty ? 2U : 0U) |
                    (static_cast<u64>(mapping.tracking_generation) << 32U);
            if (clear) {
                mapping.accessed = false;
                mapping.dirty = false;
                ++mapping.tracking_generation;
                if (mapping.tracking_generation == 0U)
                    mapping.tracking_generation = 1U;
            }
            unlock_vm(vm);
            return error_t::success;
        }
        unlock_vm(vm);
        return error_t::not_found;
    }
} // namespace sys::kernel::hypervisor
