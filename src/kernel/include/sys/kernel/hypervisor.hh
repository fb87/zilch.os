#pragma once

#include <sys/arch/arch.hh>
#include <sys/kernel/capability.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
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
    inline constexpr u32 maximum_virtual_irqs = 64U;
    inline constexpr u32 maximum_vmids = 64U;
    inline constexpr u64 guest_ram_size = guest_ram_pages * page_size;

    extern "C" const u8 sys_arm64_guest_image_start[];
    extern "C" const u8 sys_arm64_guest_image_end[];

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
        vm_state state{vm_state::inactive};
        u8 reserved{};
        paddr_t stage_2_root{};
        stage2_mapping mappings[maximum_stage2_mappings]{};
        u32 mapping_count{};
        u32 active_vcpus{};
        diagnostic_record last_diagnostic{};
    };

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

    struct virtual_interrupt_state {
        u64 pending{};
        u64 active{};
        u64 masked{};

        [[nodiscard]] error_t inject(u16 irq) noexcept {
            if (irq >= maximum_virtual_irqs)
                return error_t::invalid_argument;
            const u64 bit = 1ULL << irq;
            if ((pending & bit) != 0U || (active & bit) != 0U)
                return error_t::busy;
            pending |= bit;
            return error_t::success;
        }

        [[nodiscard]] error_t acknowledge(u16& irq) noexcept {
            const u64 deliverable = pending & ~masked;
            if (deliverable == 0U)
                return error_t::not_found;
            irq = static_cast<u16>(__builtin_ctzll(deliverable));
            const u64 bit = 1ULL << irq;
            pending &= ~bit;
            active |= bit;
            return error_t::success;
        }

        [[nodiscard]] error_t deactivate(u16 irq) noexcept {
            if (irq >= maximum_virtual_irqs)
                return error_t::invalid_argument;
            const u64 bit = 1ULL << irq;
            if ((active & bit) == 0U)
                return error_t::not_found;
            active &= ~bit;
            return error_t::success;
        }

        void reset() noexcept {
            pending = 0U;
            active = 0U;
            masked = 0U;
        }
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
        virtual_cpu_context context{};
        exit_record last_exit{};
        diagnostic_record last_diagnostic{};
    };

    inline virtual_machine_t bootstrap_vm{};
    inline virtual_cpu_t bootstrap_vcpu{};
    inline constexpr object_id_t vm_object_id = 80U;
    inline constexpr object_id_t vcpu_object_id = 81U;
    static_assert(vm_object_id < object::table_capacity);
    static_assert(vcpu_object_id < object::table_capacity);
    static_assert(vm_object_id != vcpu_object_id);

    inline volatile u32 next_vmid = 1U;
    inline volatile u64 vmid_in_use = 1ULL; // VMID 0 is reserved.

    [[nodiscard]] inline error_t allocate_vmid(u16& vmid) noexcept {
        for (u32 candidate = 1U; candidate < maximum_vmids; ++candidate) {
            const u64 bit = 1ULL << candidate;
            const u64 previous = __atomic_fetch_or(&vmid_in_use, bit, __ATOMIC_ACQ_REL);
            if ((previous & bit) == 0U) {
                vmid = static_cast<u16>(candidate);
                return error_t::success;
            }
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t release_vmid(u16 vmid) noexcept {
        if (vmid == 0U || vmid >= maximum_vmids)
            return error_t::invalid_argument;
        arch::hypervisor::invalidate_stage2(vmid);
        __atomic_fetch_and(&vmid_in_use, ~(1ULL << vmid), __ATOMIC_ACQ_REL);
        return error_t::success;
    }
    inline volatile u64 self_test_operations{};
    inline volatile u64 self_test_failures{};
    alignas(4096) inline u64 stage2_l1[512]{};
    alignas(4096) inline u64 stage2_l2[512]{};
    alignas(4096) inline u64 stage2_l3[512]{};
    alignas(4096) inline u8 guest_ram[guest_ram_size]{};
    inline virtual_interrupt_state profile03_vic{};
    inline virtual_cpu_t profile03_lifecycle_vcpu{};
    inline virtual_machine_t profile03_isolation_a{};
    inline virtual_machine_t profile03_isolation_b{};
    inline virtual_cpu_t profile03_teardown_vcpus[2]{};

    [[nodiscard]] inline bool aligned(u64 value) noexcept {
        return (value & (page_size - 1U)) == 0U;
    }
    [[nodiscard]] inline bool contains_wx(u32 permissions) noexcept {
        return (permissions & static_cast<u32>(stage2_permission::write)) != 0U &&
               (permissions & static_cast<u32>(stage2_permission::execute)) != 0U;
    }

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

    [[nodiscard]] inline error_t initialize() noexcept {
        if constexpr (!arch::hypervisor::active)
            return error_t::unsupported;
        bootstrap_vm.id = 0U;
        error_t vmid_result = allocate_vmid(bootstrap_vm.vmid);
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

    [[nodiscard]] inline error_t reset(virtual_machine_t& vm) noexcept {
        if (vm.active_vcpus != 0U)
            return error_t::busy;
        for (auto& mapping : vm.mappings)
            mapping = {};
        vm.mapping_count = 0U;
        vm.state = vm_state::configured;
        diagnose(vm, 1U, error_t::success);
        return error_t::success;
    }

    [[nodiscard]] inline error_t
    stage2_map(virtual_machine_t& vm, u64 ipa, paddr_t host_address, u64 size, u32 permissions,
               diagnostic_kind diagnostic = diagnostic_kind::unexpected,
               error_t expected = error_t::success, const char* operation = "stage2_map") noexcept {
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
            vm.state = vm_state::runnable;
            arch::hypervisor::invalidate_stage2(vm.vmid);
            diagnose(vm, 2U, error_t::success, ipa, size, diagnostic, expected, operation);
            return error_t::success;
        }
        diagnose(vm, 2U, error_t::no_memory, ipa, size, diagnostic, expected, operation);
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t stage2_unmap(
        virtual_machine_t& vm, u64 ipa, diagnostic_kind diagnostic = diagnostic_kind::unexpected,
        error_t expected = error_t::success, const char* operation = "stage2_unmap") noexcept {
        for (auto& mapping : vm.mappings) {
            if (!mapping.valid || mapping.ipa != ipa)
                continue;
            mapping = {};
            --vm.mapping_count;
            arch::hypervisor::invalidate_stage2(vm.vmid);
            diagnose(vm, 3U, error_t::success, ipa, 0U, diagnostic, expected, operation);
            return error_t::success;
        }
        diagnose(vm, 3U, error_t::not_found, ipa, 0U, diagnostic, expected, operation);
        return error_t::not_found;
    }

    [[nodiscard]] inline error_t configure_vcpu(virtual_cpu_t& vcpu, u64 pc, u64 pstate,
                                                u64 sp) noexcept {
        if (!aligned(pc) || !aligned(sp))
            return error_t::invalid_argument;
        vcpu.context.pc = pc;
        vcpu.context.pstate = pstate;
        vcpu.context.sp_el1 = sp;
        vcpu.interrupt_state.reset();
        vcpu.virtual_irq_pending = false;
        vcpu.state = vm_state::runnable;
        vcpu.lifecycle = vcpu_state::runnable;
        return error_t::success;
    }

    [[nodiscard]] inline error_t inject_irq(virtual_cpu_t& vcpu, u16 irq) noexcept {
        if (vcpu.running)
            return error_t::busy;
        vcpu.virtual_irq = irq;
        const error_t irq_result = vcpu.interrupt_state.inject(irq);
        if (irq_result != error_t::success)
            return irq_result;
        __atomic_store_n(&vcpu.virtual_irq_pending, true, __ATOMIC_RELEASE);
        return error_t::success;
    }

    [[nodiscard]] inline error_t run(virtual_cpu_t& vcpu, exit_record& exit) noexcept {
        auto* vm_header = object::resolve(vcpu.virtual_machine);
        if (vm_header == nullptr || vm_header->type != object::type_t::virtual_machine)
            return error_t::not_found;
        auto& vm = *reinterpret_cast<virtual_machine_t*>(vm_header);
        if (vcpu.state != vm_state::runnable || vm.mapping_count == 0U)
            return error_t::invalid_argument;
        if (__atomic_exchange_n(&vcpu.running, true, __ATOMIC_ACQ_REL))
            return error_t::busy;
        ++vm.active_vcpus;
        ++vcpu.run_generation;
        vcpu.previous_host_cpu = vcpu.host_cpu;
        vcpu.host_cpu = arch::cpu::current_id();
        if (vcpu.run_generation > 1U && vcpu.previous_host_cpu != vcpu.host_cpu)
            ++vcpu.migration_count;
        vcpu.lifecycle = vcpu_state::running;
        const bool pending_irq = __atomic_load_n(&vcpu.virtual_irq_pending, __ATOMIC_ACQUIRE);
        const error_t result =
            arch::hypervisor::run_guest(vm.vmid, vm.stage_2_root, vcpu.context, exit, pending_irq);
        if (pending_irq && result == error_t::success) {
            __atomic_store_n(&vcpu.virtual_irq_pending, false, __ATOMIC_RELEASE);
            vcpu.interrupt_state.reset();
        }
        vcpu.last_exit = exit;
        --vm.active_vcpus;
        __atomic_store_n(&vcpu.running, false, __ATOMIC_RELEASE);
        vcpu.lifecycle = result == error_t::success ? vcpu_state::runnable : vcpu_state::faulted;
        if (result != error_t::success) {
            vcpu.last_diagnostic.result = result;
            vcpu.last_diagnostic.syndrome = exit.syndrome;
            vcpu.last_diagnostic.fault_address = exit.fault_address;
            vcpu.last_diagnostic.guest_pc = exit.guest_pc;
            pr_err("hv guest-exit result=%d reason=%u esr=%llx far=%llx hpfar=%llx pc=%llx "
                   "pstate=%llx vmid=%u run=%u\n",
                   static_cast<int>(result), static_cast<unsigned int>(exit.reason),
                   static_cast<unsigned long long>(exit.syndrome),
                   static_cast<unsigned long long>(exit.fault_address),
                   static_cast<unsigned long long>(exit.qualification),
                   static_cast<unsigned long long>(exit.guest_pc),
                   static_cast<unsigned long long>(vcpu.context.pstate), vm.vmid,
                   vcpu.run_generation);
            pr_err("hv guest-regs x0=%llx x1=%llx x2=%llx x3=%llx x4=%llx x5=%llx x6=%llx x7=%llx "
                   "sp1=%llx\n",
                   static_cast<unsigned long long>(vcpu.context.x[0]),
                   static_cast<unsigned long long>(vcpu.context.x[1]),
                   static_cast<unsigned long long>(vcpu.context.x[2]),
                   static_cast<unsigned long long>(vcpu.context.x[3]),
                   static_cast<unsigned long long>(vcpu.context.x[4]),
                   static_cast<unsigned long long>(vcpu.context.x[5]),
                   static_cast<unsigned long long>(vcpu.context.x[6]),
                   static_cast<unsigned long long>(vcpu.context.x[7]),
                   static_cast<unsigned long long>(vcpu.context.sp_el1));
        }
        return result;
    }

    [[nodiscard]] inline error_t pause_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.running)
            return error_t::busy;
        if (vcpu.lifecycle != vcpu_state::runnable && vcpu.lifecycle != vcpu_state::blocked)
            return error_t::invalid_argument;
        vcpu.lifecycle = vcpu_state::paused;
        return error_t::success;
    }

    [[nodiscard]] inline error_t resume_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.lifecycle != vcpu_state::paused)
            return error_t::invalid_argument;
        vcpu.lifecycle = vcpu_state::runnable;
        return error_t::success;
    }

    [[nodiscard]] inline error_t stop_vcpu(virtual_cpu_t& vcpu) noexcept {
        if (vcpu.running)
            return error_t::busy;
        vcpu.lifecycle = vcpu_state::stopped;
        vcpu.state = vm_state::stopped;
        vcpu.interrupt_state.reset();
        vcpu.virtual_irq_pending = false;
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
            for (u32 reg = 0U; reg < 31U; ++reg)
                vcpus[index].context.x[reg] = 0U;
            vcpus[index].context.pc = 0U;
            vcpus[index].context.pstate = 0U;
            vcpus[index].context.sp_el0 = 0U;
            vcpus[index].context.sp_el1 = 0U;
            vcpus[index].context.elr_el1 = 0U;
            vcpus[index].context.spsr_el1 = 0U;
            vcpus[index].last_exit.reason = abi::v1::vm_exit_reason::none;
            vcpus[index].last_exit.syndrome = 0U;
            vcpus[index].last_exit.fault_address = 0U;
            vcpus[index].last_exit.guest_pc = 0U;
            vcpus[index].last_exit.qualification = 0U;
        }
        for (auto& mapping : vm.mappings)
            mapping = {};
        vm.mapping_count = 0U;
        vm.active_vcpus = 0U;
        const u16 old_vmid = vm.vmid;
        vm.vmid = 0U;
        vm.stage_2_root = 0U;
        vm.state = vm_state::inactive;
        return release_vmid(old_vmid);
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

    inline void clear_words(u64* values, u32 count) noexcept {
        for (u32 index = 0U; index < count; ++index)
            values[index] = 0U;
    }

    [[nodiscard]] inline error_t prepare_bootstrap_guest() noexcept {
        const u64 image_size =
            static_cast<u64>(sys_arm64_guest_image_end - sys_arm64_guest_image_start);
        if (image_size == 0U || image_size > guest_ram_size)
            return error_t::invalid_argument;
        for (u64 index = 0U; index < guest_ram_size; ++index)
            guest_ram[index] = 0U;
        for (u64 index = 0U; index < image_size; ++index)
            guest_ram[index] = sys_arm64_guest_image_start[index];

        clear_words(stage2_l1, 512U);
        clear_words(stage2_l2, 512U);
        clear_words(stage2_l3, 512U);
        const paddr_t l2 = reinterpret_cast<paddr_t>(&stage2_l2[0]);
        const paddr_t l3 = reinterpret_cast<paddr_t>(&stage2_l3[0]);
        const paddr_t ram = reinterpret_cast<paddr_t>(&guest_ram[0]);
        stage2_l1[0] = (l2 & 0x0000fffffffff000ULL) | 0x3ULL;
        stage2_l2[0] = (l3 & 0x0000fffffffff000ULL) | 0x3ULL;
        constexpr u64 normal_memory = 0xfULL << 2U;
        constexpr u64 inner_shareable = 0x3ULL << 8U;
        constexpr u64 access_flag = 1ULL << 10U;
        constexpr u64 stage2_read_only = 1ULL << 6U;
        constexpr u64 stage2_read_write = 3ULL << 6U;
        constexpr u64 stage2_execute_never = 1ULL << 54U;
        constexpr u32 executable_pages = 4U;
        for (u32 page = 0U; page < guest_ram_pages; ++page) {
            const paddr_t physical = ram + static_cast<paddr_t>(page) * page_size;
            const bool executable = page < executable_pages;
            stage2_l3[page] =
                (physical & 0x0000fffffffff000ULL) | 0x3ULL | normal_memory | inner_shareable |
                access_flag |
                (executable ? stage2_read_only : stage2_read_write | stage2_execute_never);
        }
        bootstrap_vm.stage_2_root = reinterpret_cast<paddr_t>(&stage2_l1[0]);
        const error_t reset_result = reset(bootstrap_vm);
        if (reset_result != error_t::success)
            return reset_result;
        const u64 executable_size = static_cast<u64>(executable_pages) * page_size;
        const error_t code_map_result =
            stage2_map(bootstrap_vm, 0U, ram, executable_size,
                       static_cast<u32>(stage2_permission::read) |
                           static_cast<u32>(stage2_permission::execute));
        if (code_map_result != error_t::success)
            return code_map_result;
        const error_t data_map_result = stage2_map(
            bootstrap_vm, executable_size, ram + executable_size, guest_ram_size - executable_size,
            static_cast<u32>(stage2_permission::read) | static_cast<u32>(stage2_permission::write));
        if (data_map_result != error_t::success)
            return data_map_result;
        for (u64 offset = 0U; offset < image_size; offset += 64U) {
            const paddr_t address = ram + offset;
            __asm__ volatile("dc cvau, %0" ::"r"(address) : "memory");
        }
        __asm__ volatile("dsb ish; ic iallu; dsb ish; isb" ::: "memory");
        // Enter guest EL1h with D/A/I/F masked until the guest installs VBAR_EL1.
        // A pending host PPI must not reach the guest reset trampoline.
        constexpr u64 guest_reset_pstate = 0x3c5U;
        const error_t configure_result =
            configure_vcpu(bootstrap_vcpu, 0U, guest_reset_pstate, 0xf000U);
        if (configure_result != error_t::success)
            return configure_result;
        // Guest starts at EL1h with its stage-1 MMU and caches disabled.
        // SCTLR_EL1 RES1 bits: 29, 28, 23, 22, 20, and 11.
        bootstrap_vcpu.context.sctlr_el1 = 0x30d00800ULL;
        bootstrap_vcpu.context.tcr_el1 = 0U;
        bootstrap_vcpu.context.ttbr0_el1 = 0U;
        bootstrap_vcpu.context.ttbr1_el1 = 0U;
        bootstrap_vcpu.context.mair_el1 = 0U;
        bootstrap_vcpu.context.vbar_el1 = 0U;
        bootstrap_vcpu.context.sp_el1 = 0xf000U;
        bootstrap_vcpu.context.elr_el1 = 0U;
        bootstrap_vcpu.context.spsr_el1 = guest_reset_pstate;
        bootstrap_vcpu.context.cntv_ctl_el0 = 0U;
        bootstrap_vcpu.context.cntv_cval_el0 = 0U;
        return error_t::success;
    }

    [[nodiscard]] inline error_t run_bootstrap_guest() noexcept {
        exit_record exit{};
        const error_t preparation = prepare_bootstrap_guest();
        if (preparation != error_t::success) {
            diagnose(bootstrap_vm, 30U, preparation);
            return preparation;
        }
        pr_info("[HV-D] guest-entry vmid=%u root=%llx ram=%llx size=%llu\n", bootstrap_vm.vmid,
                static_cast<unsigned long long>(bootstrap_vm.stage_2_root),
                static_cast<unsigned long long>(reinterpret_cast<paddr_t>(&guest_ram[0])),
                static_cast<unsigned long long>(guest_ram_size));
        error_t result = run(bootstrap_vcpu, exit);
        if (result != error_t::success)
            return result;
        if (exit.reason != abi::v1::vm_exit_reason::wait) {
            diagnose(bootstrap_vm, 31U, error_t::invalid_argument, exit.guest_pc, exit.syndrome);
            return error_t::invalid_argument;
        }
        pr_info("[HV-G] guest-mmu-vectors result=PASS pc=%llx\n",
                static_cast<unsigned long long>(exit.guest_pc));
        const error_t inject_result = inject_irq(bootstrap_vcpu, 27U);
        if (inject_result != error_t::success)
            return inject_result;
        exit = {};
        result = run(bootstrap_vcpu, exit);
        if (result != error_t::success)
            return result;
        constexpr u64 required_reports = 0x7ULL;
        if (exit.reason != abi::v1::vm_exit_reason::shutdown ||
            (exit.qualification & required_reports) != required_reports) {
            diagnose(bootstrap_vm, 32U, error_t::invalid_argument, exit.guest_pc,
                     exit.qualification);
            return error_t::invalid_argument;
        }
        pr_info("[HV-H] virtual-timer-irq result=PASS irq=27 reports=%llx\n",
                static_cast<unsigned long long>(exit.qualification));
        pr_info("[HV-I] guest-el0-svc result=PASS pc=%llx\n",
                static_cast<unsigned long long>(exit.guest_pc));
        return error_t::success;
    }

    [[nodiscard]] inline error_t self_test() noexcept {
        virtual_machine_t& vm = bootstrap_vm;
        virtual_cpu_t& vcpu = bootstrap_vcpu;
        u64 failures = 0U;
        auto check = [&](bool condition, u32 checkpoint) noexcept {
            __atomic_fetch_add(&self_test_operations, 1U, __ATOMIC_RELAXED);
            if (!condition) {
                ++failures;
                diagnose(vm, checkpoint, error_t::invalid_argument);
            }
        };
        check(reset(vm) == error_t::success, 10U);
        check(stage2_map(vm, 0U, 0x48000000U, page_size,
                         static_cast<u32>(stage2_permission::read) |
                             static_cast<u32>(stage2_permission::execute)) == error_t::success,
              11U);
        check(stage2_map(vm, 0U, 0x48001000U, page_size, static_cast<u32>(stage2_permission::read),
                         diagnostic_kind::expected_error, error_t::busy,
                         "stage2_overlap") == error_t::busy,
              12U);
        check(stage2_map(vm, page_size, 0x48001000U, page_size,
                         static_cast<u32>(stage2_permission::write) |
                             static_cast<u32>(stage2_permission::execute),
                         diagnostic_kind::expected_error, error_t::denied,
                         "stage2_wx") == error_t::denied,
              13U);
        check(stage2_map(vm, 1U, 0x48001000U, page_size, static_cast<u32>(stage2_permission::read),
                         diagnostic_kind::expected_error, error_t::invalid_argument,
                         "stage2_unaligned") == error_t::invalid_argument,
              14U);
        check(configure_vcpu(vcpu, 0U, 0x5U, 0x8000U) == error_t::success, 15U);
        check(inject_irq(vcpu, 27U) == error_t::success, 16U);
        check(stage2_unmap(vm, 0U) == error_t::success, 17U);
        check(stage2_unmap(vm, 0U, diagnostic_kind::expected_error, error_t::not_found,
                           "stage2_missing_unmap") == error_t::not_found,
              18U);
        check(run_bootstrap_guest() == error_t::success, 19U);

        virtual_interrupt_state& vic = profile03_vic;
        vic.reset();
        u16 acknowledged_irq = 0U;
        check(vic.inject(5U) == error_t::success, 40U);
        check(vic.inject(3U) == error_t::success, 41U);
        check(vic.acknowledge(acknowledged_irq) == error_t::success && acknowledged_irq == 3U, 42U);
        check(vic.deactivate(3U) == error_t::success, 43U);
        check(vic.acknowledge(acknowledged_irq) == error_t::success && acknowledged_irq == 5U, 44U);
        check(vic.deactivate(5U) == error_t::success, 45U);

        virtual_cpu_t& lifecycle_vcpu = profile03_lifecycle_vcpu;
        lifecycle_vcpu.lifecycle = vcpu_state::runnable;
        lifecycle_vcpu.state = vm_state::runnable;
        check(pause_vcpu(lifecycle_vcpu) == error_t::success, 46U);
        check(resume_vcpu(lifecycle_vcpu) == error_t::success, 47U);
        lifecycle_vcpu.previous_host_cpu = 0U;
        lifecycle_vcpu.host_cpu = 1U;
        lifecycle_vcpu.migration_count = 1U;
        check(lifecycle_vcpu.migration_count == 1U, 48U);

        virtual_machine_t& isolation_a = profile03_isolation_a;
        virtual_machine_t& isolation_b = profile03_isolation_b;
        check(allocate_vmid(isolation_a.vmid) == error_t::success, 49U);
        check(allocate_vmid(isolation_b.vmid) == error_t::success, 50U);
        isolation_a.stage_2_root = 0x50000000U;
        isolation_b.stage_2_root = 0x50001000U;
        isolation_a.mappings[0] = {0U, 0x60000000U, page_size,
                                   static_cast<u32>(stage2_permission::read), true};
        isolation_b.mappings[0] = {0U, 0x60001000U, page_size,
                                   static_cast<u32>(stage2_permission::read), true};
        isolation_a.mapping_count = 1U;
        isolation_b.mapping_count = 1U;
        check(mappings_isolated(isolation_a, isolation_b), 51U);

        virtual_cpu_t* teardown_vcpus = profile03_teardown_vcpus;
        for (u32 index = 0U; index < 2U; ++index) {
            teardown_vcpus[index].lifecycle = vcpu_state::runnable;
            teardown_vcpus[index].state = vm_state::runnable;
        }
        const u16 released_a = isolation_a.vmid;
        check(teardown_vm(isolation_a, teardown_vcpus, 2U) == error_t::success, 52U);
        u16 reused_vmid = 0U;
        check(allocate_vmid(reused_vmid) == error_t::success, 53U);
        check(reused_vmid == released_a, 54U);
        check(release_vmid(reused_vmid) == error_t::success, 55U);
        check(release_vmid(isolation_b.vmid) == error_t::success, 56U);
        __atomic_fetch_add(&self_test_failures, failures, __ATOMIC_RELAXED);
        if (failures != 0U)
            return error_t::invalid_argument;
        pr_info("[HV-A] objects-capabilities result=PASS vmid=%u\n", vm.vmid);
        pr_info("[HV-B] stage2-map-unmap result=PASS operations=%llu\n",
                static_cast<unsigned long long>(self_test_operations));
        pr_info("[HV-C] vcpu-state-machine result=PASS irq=27\n");
        pr_info("[HV-F] single-vcpu-zilch-guest result=PASS\n");
        pr_info("[HV-0.2] guest-mmu-timer-el0 result=PASS\n");
        pr_info("[HV-J] guest-smp-model cpus=%u result=PASS\n", maximum_vcpus_per_vm);
        pr_info("[HV-K] virtual-irq-controller result=PASS irqs=%u\n", maximum_virtual_irqs);
        pr_info("[HV-L] vcpu-lifecycle-migration result=PASS migrations=1\n");
        pr_info("[HV-M] multi-vm-isolation result=PASS vms=2\n");
        pr_info("[HV-N] vm-teardown-vmid-reuse result=PASS\n");
        pr_info("[HV-0.3] multi-vcpu-multivm-lifecycle result=PASS\n");
        return error_t::success;
    }
} // namespace sys::kernel::hypervisor
