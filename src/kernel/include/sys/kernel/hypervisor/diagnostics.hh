#pragma once

#include <sys/kernel/hypervisor/object.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::hypervisor
{
    enum class diagnostic_kind : u8 { unexpected, expected_error };

    inline void diagnose(virtual_machine_t& vm, u32 checkpoint, error_t result, u64 ipa = 0U,
                         u64 value = 0U, diagnostic_kind kind = diagnostic_kind::unexpected,
                         error_t expected = error_t::success,
                         const char* operation = "hypervisor") noexcept {
        vm.last_diagnostic = {diagnostic_magic,
                              checkpoint,
                              result,
                              vm.object.generation,
                              bootstrap_vcpu.object.generation,
                              vm.vmid,
                              0U,
                              ipa,
                              value,
                              0U,
                              0U,
                              bootstrap_vcpu.context.pc};
        if (kind == diagnostic_kind::expected_error) {
            if (result == expected) {
                pr_info("[TEST] expected-error operation=%s expected=%d actual=%d result=PASS "
                        "ipa=%llx value=%llx\n",
                        operation, static_cast<int>(expected), static_cast<int>(result),
                        static_cast<unsigned long long>(ipa),
                        static_cast<unsigned long long>(value));
            } else {
                pr_err("[TEST] expected-error operation=%s expected=%d actual=%d result=FAIL "
                       "checkpoint=%u vmid=%u ipa=%llx value=%llx\n",
                       operation, static_cast<int>(expected), static_cast<int>(result), checkpoint,
                       static_cast<unsigned int>(vm.vmid), static_cast<unsigned long long>(ipa),
                       static_cast<unsigned long long>(value));
            }
        } else if (result != error_t::success) {
            pr_err("hv checkpoint=%u result=%d vm=%llu gen=%u vmid=%u ipa=%llx value=%llx "
                   "mappings=%u state=%u\n",
                   checkpoint, static_cast<int>(result), static_cast<unsigned long long>(vm.id),
                   vm.object.generation, static_cast<unsigned int>(vm.vmid),
                   static_cast<unsigned long long>(ipa), static_cast<unsigned long long>(value),
                   vm.mapping_count, static_cast<unsigned int>(vm.state));
        }
    }
} // namespace sys::kernel::hypervisor
