#pragma once

#include <sys/kernel/hypervisor/virtual_irq.hh>
#include <sys/kernel/hypervisor/virtual_timer.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

#include <abi/sys/v1/hypervisor.hh>

namespace sys::kernel::hypervisor
{
    inline constexpr u32 maximum_stage2_mappings = 16U;
    inline constexpr u64 page_size = 4096U;
    inline constexpr u64 guest_ipa_limit = 1ULL << 32U;
    inline constexpr u32 diagnostic_magic = 0x48563031U; // HV01
    inline constexpr u32 guest_ram_pages = 16U;
    inline constexpr u32 maximum_vcpus_per_vm = 4U;
    inline constexpr u32 maximum_vmids = 64U;
    inline constexpr u64 guest_ram_size = guest_ram_pages * page_size;

    enum class vm_state : u8 { inactive, configured, runnable, running, stopped, faulted };

    enum class stage2_permission : u32 {
        none = 0U,
        read = 1U << 0U,
        write = 1U << 1U,
        execute = 1U << 2U,
        device = 1U << 3U,
    };

    struct stage2_mapping {
        u64 ipa{};
        paddr_t host_address{};
        u64 size{};
        u32 permissions{};
        bool valid{};
    };

    struct diagnostic_record {
        u32 magic{diagnostic_magic};
        u32 checkpoint{};
        error_t result{error_t::success};
        u32 vm_generation{};
        u32 vcpu_generation{};
        u16 vmid{};
        u16 reserved{};
        u64 ipa{};
        u64 value{};
        u64 syndrome{};
        u64 fault_address{};
        u64 guest_pc{};
    };

    struct virtual_machine_t {
        object::header_t object{};
        volatile u32 lock{};
        vm_id_t id{};
        u16 vmid{};
        u32 vmid_generation{};
        vm_state state{vm_state::inactive};
        u8 reserved{};
        paddr_t stage_2_root{};
        stage2_mapping mappings[maximum_stage2_mappings]{};
        u32 mapping_count{};
        u32 active_vcpus{};
        u64 mapped_pages{};
        u64 peak_mapped_pages{};
        u64 map_operations{};
        u64 unmap_operations{};
        u64 run_entries{};
        u64 run_exits{};
        u64 counter_offset{};
        u64 accounting_faults{};
        diagnostic_record last_diagnostic{};
    };

    enum class audit_action : u16 {
        reset = 1U,
        map = 2U,
        unmap = 3U,
        pause = 4U,
        resume = 5U,
        stop = 6U,
        teardown = 7U,
        run_enter = 8U,
        run_exit = 9U,
    };

    struct audit_record {
        u64 sequence{};
        audit_action action{};
        u16 vmid{};
        vm_id_t vm{};
        u64 value{};
    };

    inline constexpr u32 audit_record_count = 64U;
    inline audit_record audit_records[audit_record_count]{};
    inline volatile u64 audit_sequence{};

    inline void audit(const virtual_machine_t& vm, audit_action action, u64 value = 0U) noexcept {
        const u64 sequence = __atomic_fetch_add(&audit_sequence, 1U, __ATOMIC_RELAXED) + 1U;
        audit_record& record = audit_records[sequence % audit_record_count];
        record.action = action;
        record.vmid = vm.vmid;
        record.vm = vm.id;
        record.value = value;
        __atomic_store_n(&record.sequence, sequence, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool accounting_valid(const virtual_machine_t& vm) noexcept {
        return vm.accounting_faults == 0U && vm.mapped_pages <= vm.peak_mapped_pages &&
               vm.run_exits <= vm.run_entries;
    }

    struct virtual_cpu_context {
        u64 x[31]{};
        u64 pc{};
        u64 pstate{};
        u64 sp_el0{};
        u64 sp_el1{};
        u64 elr_el1{};
        u64 spsr_el1{};
        u64 sctlr_el1{};
        u64 tcr_el1{};
        u64 ttbr0_el1{};
        u64 ttbr1_el1{};
        u64 mair_el1{};
        u64 vbar_el1{};
        u64 tpidr_el0{};
        u64 tpidr_el1{};
        u64 cntv_ctl_el0{};
        u64 cntv_cval_el0{};
    };

    struct exit_record {
        abi::v1::vm_exit_reason reason{abi::v1::vm_exit_reason::none};
        u64 syndrome{};
        u64 fault_address{};
        u64 guest_pc{};
        u64 qualification{};
    };

    enum class vcpu_state : u8 {
        inactive,
        configured,
        runnable,
        running,
        blocked,
        paused,
        stopped,
        faulted
    };

    struct virtual_cpu_t {
        object::header_t object{};
        volatile u32 lock{};
        vcpu_id_t id{};
        object::reference_t virtual_machine{};
        vm_state state{vm_state::inactive};
        vcpu_state lifecycle{vcpu_state::inactive};
        cpu_id_t host_cpu{};
        cpu_id_t previous_host_cpu{};
        u32 migration_count{};
        bool running{};
        bool virtual_irq_pending{};
        u16 virtual_irq{};
        virtual_interrupt_state interrupt_state{};
        u32 run_generation{};
        u32 logical_id{};
        u32 executed_quanta{};
        u32 virtual_ipi_count{};
        virtual_timer_state timer{};
        virtual_cpu_context context{};
        exit_record last_exit{};
        diagnostic_record last_diagnostic{};
    };

    inline virtual_machine_t bootstrap_vm{};
    inline virtual_cpu_t bootstrap_vcpu{};
    inline constexpr object_id_t vm_object_id = object::bootstrap_id::virtual_machine;
    inline constexpr object_id_t vcpu_object_id = object::bootstrap_id::virtual_cpu;
    static_assert(vm_object_id < object::table_capacity);
    static_assert(vcpu_object_id < object::table_capacity);
    static_assert(vm_object_id != vcpu_object_id);
} // namespace sys::kernel::hypervisor
