#pragma once

#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/hypervisor.hh>
#include <sys/kernel/interrupt.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>
#include <abi/sys/v1/syscall_numbers.hh>
#if CONFIG_HYPERVISOR_SELFTEST
#include <sys/kernel/tests/hypervisor/control_models.hh>
#endif
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduling/context.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/platform.hh>
#include <sys/types.hh>
#if CONFIG_SELFTEST
#include <sys/test_abi/v1/certification.hh>
#endif

namespace sys::kernel::syscall
{
    [[nodiscard]] inline bool process_lifecycle_valid() noexcept {
        for (u32 index = 0U; index < thread::user_thread_count; ++index) {
            const thread::thread& value = thread::user_threads[index];
            const task::task& owner = thread::user_tasks[index];
            const thread::state current = thread::load_state(value);
            if (value.object.type == object::type_t::none) {
                if (current != thread::state::inactive || value.owner != nullptr ||
                    owner.object.type != object::type_t::none ||
                    value.address_space.object.type != object::type_t::none ||
                    value.scheduling_context.object.type != object::type_t::none)
                    return false;
                continue;
            }
            if (value.object.type != object::type_t::thread || value.owner != &owner ||
                owner.object.type != object::type_t::task ||
                value.address_space.object.type != object::type_t::address_space ||
                value.scheduling_context.object.type != object::type_t::scheduling_context ||
                value.pinned_cpu >= thread::maximum_cpu_count ||
                value.scheduling_context.affinity >= thread::maximum_cpu_count ||
                value.scheduling_context.donation_depth > scheduling::maximum_donation_depth ||
                value.scheduling_context.replenishment_count > scheduling::maximum_replenishments ||
                current == thread::state::inactive)
                return false;
            if (value.reply.valid && (value.reply.caller >= thread::user_thread_count ||
                                      thread::user_threads[value.reply.caller].object.generation !=
                                          value.reply.generation))
                return false;
        }
        return true;
    }

    [[nodiscard]] inline bool scheduler_database_valid() noexcept {
        for (u32 index = 0U; index < thread::active_user_thread_count; ++index) {
            const thread::thread& value = thread::user_threads[index];
            if (value.object.type == object::type_t::none)
                continue;
            const scheduling::context& context = value.scheduling_context;
            if (value.pinned_cpu >= thread::maximum_cpu_count ||
                context.affinity != value.pinned_cpu || context.budget_ticks == 0U ||
                context.period_ticks == 0U || context.budget_ticks > context.period_ticks ||
                context.effective_priority > context.maximum_priority ||
                context.donation_depth > scheduling::maximum_donation_depth ||
                context.replenishment_count > scheduling::maximum_replenishments ||
                context.consumed_ticks > context.budget_ticks)
                return false;
            u64 accounted{};
            u64 previous{};
            for (u32 entry = 0U; entry < context.replenishment_count; ++entry) {
                if (context.replenishment_amounts[entry] == 0U ||
                    (entry != 0U && context.replenishment_deadlines[entry] < previous) ||
                    accounted > ~0ULL - context.replenishment_amounts[entry])
                    return false;
                previous = context.replenishment_deadlines[entry];
                accounted += context.replenishment_amounts[entry];
            }
            if (accounted < context.consumed_ticks ||
                (context.replenishment_count == 0U &&
                 context.next_replenishment != scheduling::maximum_time))
                return false;
        }
        for (u32 cpu = 0U; cpu < thread::maximum_cpu_count; ++cpu) {
            if (thread::timeout_queue_counts[cpu] > thread::user_thread_count)
                return false;
            u64 previous{};
            for (u32 entry = 0U; entry < thread::timeout_queue_counts[cpu]; ++entry) {
                const thread::timeout_entry& timeout = thread::timeout_queues[cpu][entry];
                if (timeout.thread >= thread::active_user_thread_count ||
                    (entry != 0U && timeout.deadline < previous))
                    return false;
                previous = timeout.deadline;
            }
        }
        return true;
    }

    inline void set_control_result(arch::thread::context& frame, error_t result) noexcept {
        arch::syscall::set_result(frame, static_cast<word_t>(static_cast<s64>(result)));
    }

    [[nodiscard]] inline error_t resolve_task(thread::thread& current, capability_id_t selector,
                                              capability::right_t right,
                                              task::task*& result) noexcept {
        result = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(current.owner->cspace, selector,
                                                  object::type_t::task, right, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<task::task*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_thread(thread::thread& current, capability_id_t selector,
                                                capability::right_t right,
                                                thread::thread*& result) noexcept {
        result = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(current.owner->cspace, selector,
                                                  object::type_t::thread, right, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<thread::thread*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_space(thread::thread& current, capability_id_t selector,
                                               space::address_space*& result) noexcept {
        result = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup =
            capability::lookup(current.owner->cspace, selector, object::type_t::address_space,
                               capability::right_t::control, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<space::address_space*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_frame(thread::thread& current, capability_id_t selector,
                                               capability::right_t right,
                                               memory::frame*& result) noexcept {
        result = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(current.owner->cspace, selector,
                                                  object::type_t::frame, right, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<memory::frame*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_vm(thread::thread& current, capability_id_t selector,
                                            capability::right_t right,
                                            hypervisor::virtual_machine_t*& result) noexcept {
        result = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(current.owner->cspace, selector,
                                                  object::type_t::virtual_machine, right, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<hypervisor::virtual_machine_t*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_vcpu(thread::thread& current, capability_id_t selector,
                                              capability::right_t right,
                                              hypervisor::virtual_cpu_t*& result) noexcept {
        result = nullptr;
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(current.owner->cspace, selector,
                                                  object::type_t::virtual_cpu, right, header);
        if (lookup != error_t::success)
            return lookup;
        result = reinterpret_cast<hypervisor::virtual_cpu_t*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t dispatch_capability_operation(
        thread::thread& current, abi::v1::control_operation operation,
        capability_id_t destination_task_selector, capability_id_t destination_selector,
        capability_id_t source_selector, u32 rights, capability::badge_t badge) noexcept {
        if (current.owner == nullptr)
            return error_t::denied;
        task::task* destination_task = current.owner;
        if (destination_task_selector != 0U) {
            const error_t lookup = resolve_task(current, destination_task_selector,
                                                capability::right_t::control, destination_task);
            if (lookup != error_t::success)
                return lookup;
        }
        capability::lock_authority();
        error_t authority_result = error_t::unsupported;
        switch (operation) {
            case abi::v1::control_operation::capability_copy:
                authority_result =
                    capability::copy_locked(destination_task->cspace, destination_selector,
                                            current.owner->cspace, source_selector, {rights});
                break;
            case abi::v1::control_operation::capability_mint:
                authority_result = capability::mint_locked(
                    destination_task->cspace, destination_selector, current.owner->cspace,
                    source_selector, {rights}, badge);
                break;
            case abi::v1::control_operation::capability_move:
                authority_result =
                    capability::move_locked(destination_task->cspace, destination_selector,
                                            current.owner->cspace, source_selector);
                break;
            case abi::v1::control_operation::capability_delete: {
                if (destination_selector >= capability::cspace_slot_count) {
                    authority_result = error_t::invalid_argument;
                    break;
                }
                capability::lock(destination_task->cspace);
                const capability::slot_t deleted =
                    capability::slot_at(destination_task->cspace, destination_selector);
                capability::unlock(destination_task->cspace);
                if (deleted.object.type != object::type_t::none &&
                    capability::derivation_valid(deleted.derivation, deleted.object))
                    (void)memory::unmap_authority(deleted.derivation, false);
                authority_result = capability::delete_capability_locked(destination_task->cspace,
                                                                        destination_selector);
                break;
            }
            case abi::v1::control_operation::capability_revoke: {
                if (source_selector >= capability::cspace_slot_count) {
                    authority_result = error_t::invalid_argument;
                    break;
                }
                capability::lock(current.owner->cspace);
                const capability::slot_t source_slot =
                    capability::slot_at(current.owner->cspace, source_selector);
                capability::unlock(current.owner->cspace);
                if (source_slot.object.type == object::type_t::none ||
                    !capability::derivation_valid(source_slot.derivation, source_slot.object)) {
                    authority_result = error_t::not_found;
                    break;
                }
                if (!source_slot.rights.contains(capability::right_t::grant) &&
                    !source_slot.rights.contains(capability::right_t::manage)) {
                    authority_result = error_t::denied;
                    break;
                }
                (void)memory::unmap_authority(source_slot.derivation, true);
                (void)capability::revoke_descendants_locked(source_slot.derivation);
                authority_result = error_t::success;
                break;
            }
            default:
                authority_result = error_t::unsupported;
                break;
        }
        capability::unlock_authority();
        return authority_result;
    }

    [[nodiscard]] inline bool dispatch_control(thread::thread& current,
                                               arch::thread::context& frame, u64 vector,
                                               u64 syndrome) noexcept {
        if (!arch::syscall::is_user_syscall(vector, syndrome))
            return false;
        if (arch::syscall::number(frame) != static_cast<word_t>(abi::v1::syscall::control))
            return false;

        const word_t raw_operation = arch::syscall::argument(frame, 0U);
#if CONFIG_SELFTEST
        const auto test_operation = static_cast<test_abi::v1::control_operation>(raw_operation);
        switch (test_operation) {
            case test_abi::v1::control_operation::acceptance_report: {
                if (current.owner == nullptr || !current.owner->root) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                const word_t test_id = arch::syscall::argument(frame, 1U);
                const word_t passed = arch::syscall::argument(frame, 2U);
                const char* name = "unknown";
                switch (test_id) {
                    case 1U:
                        name = "root_only_boot";
                        break;
                    case 2U:
                        name = "bootinfo_contract";
                        break;
                    case 3U:
                        name = "capability_control";
                        break;
                    case 4U:
                        name = "notification_control";
                        break;
                    case 5U:
                        name = "root_created_objects";
                        break;
                    case 6U:
                        name = "root_created_smp_fuzz";
                        break;
                    case 7U:
                        name = "object_destroy_reuse";
                        break;
                    case 8U:
                        name = "hypervisor_real_single_vcpu";
                        break;
                    case 9U:
                        name = "hypervisor_negative_fuzz";
                        break;
                    case 10U:
                        name = "hypervisor_control_model_0_4";
                        break;
                    case 11U:
                        name = "userspace_pager_service";
                        break;
                    case 12U:
                        name = "dynamic_ipc_objects";
                        break;
                    case 13U:
                        name = "memory_resource_lifecycle";
                        break;
                    case 14U:
                        name = "memory_mapping_database";
                        break;
                    case 15U:
                        name = "memory_authority_revoke";
                        break;
                    case 16U:
                        name = "memory_attributes_pressure";
                        break;
                    case 17U:
                        name = "memory_resource_delegation";
                        break;
                    case 18U:
                        name = "memory_extent_retype";
                        break;
                    case 19U:
                        name = "memory_extent_metadata";
                        break;
                    case 20U:
                        name = "memory_pressure_rollback";
                        break;
                    case 21U:
                        name = "memory_server_protocol";
                        break;
                    case 22U:
                        name = "ipc_lifecycle_races";
                        break;
                    case 23U:
                        name = "capability_transfer_revoke_race";
                        break;
                    case 24U:
                        name = "object_lookup_destroy_race";
                        break;
                    case 25U:
                        name = "scheduling_configuration";
                        break;
                    case 26U:
                        name = "ipc_capability_batch";
                        break;
                    case 27U:
                        name = "ipc_ool_frame_grant";
                        break;
                    case 28U:
                        name = "ipc_completion_gate";
                        break;
                    case 29U:
                        name = "memory_completion_gate";
                        break;
                    case 30U:
                        name = "capability_completion_gate";
                        break;
                    case 31U:
                        name = "scheduler_completion_gate";
                        break;
                    case 32U:
                        name = "interrupt_timer_platform_gate";
                        break;
                    case 33U:
                        name = "security_hardening_gate";
                        break;
                    case 34U:
                        name = "kernel_core_1_0_gate";
                        break;
                    default:
                        break;
                }
                pr_info("[TEST] name=%s result=%s\n", name, passed != 0U ? "PASS" : "FAIL");
                set_control_result(frame,
                                   passed != 0U ? error_t::success : error_t::invalid_argument);
                return true;
            }
            case test_abi::v1::control_operation::acceptance_finalize: {
                if (current.owner == nullptr || !current.owner->root) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                const word_t passed = arch::syscall::argument(frame, 1U);
                const word_t failures = arch::syscall::argument(frame, 2U);
                const word_t failure_mask = arch::syscall::argument(frame, 3U);
                const word_t transport_ok = arch::syscall::argument(frame, 4U);
                const bool mappings_valid = memory::mapping_database_valid();
                const bool objects_valid = object::accounting_valid();
                const bool locks_valid = lock_order::violation_count() == 0U;
                const bool endpoints_valid = ipc::database_valid();
                const bool notifications_valid = notification::database_valid();
                const bool interrupts_valid = interrupt::database_valid();
                const bool processes_valid = process_lifecycle_valid();
                const bool capabilities_valid = capability::database_valid();
                const bool scheduler_valid = scheduler_database_valid();
                const bool platform_valid = platform::certification_valid();
                const bool timers_valid =
                    platform::timer::database_valid(arch::smp::online_count());
                const bool architecture_hardening_valid =
                    arch::hardening::inventory_valid(arch::smp::online_count()) &&
                    arch::memory::architectural_controls_valid() &&
                    arch::memory::kernel_permissions_valid() &&
                    arch::memory::kernel_stack_guards_valid() &&
                    arch::memory::privilege_protection_enabled();
                const bool failure_state_valid =
                    emergency::verify_ring() && emergency::crash_valid();
                const bool memory_inventory_valid = memory::physical_inventory_source !=
                                                    memory::inventory_source::platform_fallback;
                const u64 ipc_latency_samples = interrupt::timing::latency_sample_count(
                    interrupt::timing::latency_kind::ipc_service);
                const u64 ipc_latency_max =
                    interrupt::timing::latency_max(interrupt::timing::latency_kind::ipc_service);
                const bool ipc_timing_valid =
                    ipc_latency_samples != 0U &&
                    ipc_latency_max <= interrupt::timing::latency_target_ticks();
                const bool scheduler_timing_valid =
                    interrupt::timing::within_target() &&
                    interrupt::timing::latency_within_target(
                        interrupt::timing::latency_kind::interrupt_service) &&
                    interrupt::timing::latency_within_target(
                        interrupt::timing::latency_kind::preemption_service) &&
                    interrupt::timing::latency_within_target(
                        interrupt::timing::latency_kind::cross_cpu_wake) &&
                    ipc_timing_valid;
                const bool kernel_invariants =
                    mappings_valid && objects_valid && locks_valid && endpoints_valid &&
                    notifications_valid && interrupts_valid && processes_valid &&
                    capabilities_valid && scheduler_valid && platform_valid && timers_valid &&
                    architecture_hardening_valid && failure_state_valid && memory_inventory_valid &&
                    scheduler_timing_valid;
                const bool acceptance_passed = passed != 0U && kernel_invariants;
                pr_info("[TEST] name=kernel_lifetime_invariants result=%s mappings=%s "
                        "objects=%s locks=%s endpoints=%s notifications=%s\n",
                        kernel_invariants ? "PASS" : "FAIL", mappings_valid ? "PASS" : "FAIL",
                        objects_valid ? "PASS" : "FAIL", locks_valid ? "PASS" : "FAIL",
                        endpoints_valid ? "PASS" : "FAIL", notifications_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=interrupt_lifetime_invariants result=%s\n",
                        interrupts_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=process_lifecycle_invariants result=%s\n",
                        processes_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=capability_database_invariants result=%s\n",
                        capabilities_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=scheduler_database_invariants result=%s\n",
                        scheduler_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=scheduler_latency_bounds result=%s limit_ticks=%llu\n",
                        scheduler_timing_valid ? "PASS" : "FAIL",
                        static_cast<unsigned long long>(interrupt::timing::latency_target_ticks()));
                pr_info("[TEST] name=platform_inventory_invariants result=%s\n",
                        platform_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=timer_database_invariants result=%s cpus=%u\n",
                        timers_valid ? "PASS" : "FAIL",
                        static_cast<unsigned int>(arch::smp::online_count()));
                pr_info("[TEST] name=architecture_hardening_invariants result=%s\n",
                        architecture_hardening_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=failure_state_invariants result=%s\n",
                        failure_state_valid ? "PASS" : "FAIL");
                pr_info("[TEST] name=ipc_latency_bound result=%s max_ticks=%llu limit_ticks=%llu "
                        "samples=%llu\n",
                        ipc_timing_valid ? "PASS" : "FAIL",
                        static_cast<unsigned long long>(ipc_latency_max),
                        static_cast<unsigned long long>(interrupt::timing::latency_target_ticks()),
                        static_cast<unsigned long long>(ipc_latency_samples));
                pr_info("[TEST] name=memory_inventory_invariants result=%s source=%u regions=%u "
                        "pages=%u\n",
                        memory_inventory_valid ? "PASS" : "FAIL",
                        static_cast<unsigned int>(memory::physical_inventory_source),
                        static_cast<unsigned int>(memory::physical_region_count),
                        static_cast<unsigned int>(memory::managed_pages));
                pr_info("[METRIC] name=irq_disabled_duration_final max_ticks=%llu "
                        "reference_ticks=%llu samples=%llu\n",
                        static_cast<unsigned long long>(interrupt::timing::maximum()),
                        static_cast<unsigned long long>(interrupt::timing::target_ticks()),
                        static_cast<unsigned long long>(interrupt::timing::sample_count()));
                pr_info("[METRIC] name=interrupt_latency max_ticks=%llu reference_ticks=%llu "
                        "samples=%llu\n",
                        static_cast<unsigned long long>(interrupt::timing::latency_max(
                            interrupt::timing::latency_kind::interrupt_service)),
                        static_cast<unsigned long long>(interrupt::timing::latency_target_ticks()),
                        static_cast<unsigned long long>(interrupt::timing::latency_sample_count(
                            interrupt::timing::latency_kind::interrupt_service)));
                pr_info("[METRIC] name=preemption_latency max_ticks=%llu reference_ticks=%llu "
                        "samples=%llu\n",
                        static_cast<unsigned long long>(interrupt::timing::latency_max(
                            interrupt::timing::latency_kind::preemption_service)),
                        static_cast<unsigned long long>(interrupt::timing::latency_target_ticks()),
                        static_cast<unsigned long long>(interrupt::timing::latency_sample_count(
                            interrupt::timing::latency_kind::preemption_service)));
                pr_info("[METRIC] name=cross_cpu_wake_latency max_ticks=%llu "
                        "reference_ticks=%llu samples=%llu\n",
                        static_cast<unsigned long long>(interrupt::timing::latency_max(
                            interrupt::timing::latency_kind::cross_cpu_wake)),
                        static_cast<unsigned long long>(interrupt::timing::latency_target_ticks()),
                        static_cast<unsigned long long>(interrupt::timing::latency_sample_count(
                            interrupt::timing::latency_kind::cross_cpu_wake)));
                pr_info("[METRIC] name=ipc_latency max_ticks=%llu reference_ticks=%llu "
                        "samples=%llu\n",
                        static_cast<unsigned long long>(interrupt::timing::latency_max(
                            interrupt::timing::latency_kind::ipc_service)),
                        static_cast<unsigned long long>(interrupt::timing::latency_target_ticks()),
                        static_cast<unsigned long long>(interrupt::timing::latency_sample_count(
                            interrupt::timing::latency_kind::ipc_service)));
                pr_info("[ACCEPTANCE] suite=root-only boot=root-only result=%s failures=%llu "
                        "failure_mask=%llx transport=%s\n",
                        acceptance_passed ? "PASS" : "FAIL",
                        static_cast<unsigned long long>(failures),
                        static_cast<unsigned long long>(failure_mask),
                        transport_ok != 0U ? "PASS" : "FAIL");
                set_control_result(frame, acceptance_passed ? error_t::success
                                                            : error_t::invalid_argument);
                return true;
            }
            case test_abi::v1::control_operation::memory_server_protocol_detail: {
                if (current.owner == nullptr || !current.owner->root) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                const word_t detail = arch::syscall::argument(frame, 1U);
                const bool client_origin = (detail & (1ULL << 31U)) != 0U;
                const bool teardown_origin = (detail & (1ULL << 30U)) != 0U;
                const bool server_teardown = (detail & (1ULL << 29U)) != 0U;
                const bool timeout_origin = (detail & (1ULL << 28U)) != 0U;
                const char* origin = client_origin     ? "client"
                                     : teardown_origin ? "client-teardown"
                                     : server_teardown ? "server-teardown"
                                     : timeout_origin  ? "timeout"
                                                       : "server";
                pr_err("[TEST-DETAIL] name=memory_server_protocol code=%llx origin=%s client=%llu "
                       "stage=%llu error=%llu\n",
                       static_cast<unsigned long long>(detail), origin,
                       static_cast<unsigned long long>(
                           (client_origin || teardown_origin) ? ((detail >> 24U) & 0x1fU) : 0U),
                       static_cast<unsigned long long>((client_origin || teardown_origin)
                                                           ? ((detail >> 16U) & 0xffU)
                                                           : (detail & 0xffU)),
                       static_cast<unsigned long long>(detail & 0xffffU));
                set_control_result(frame, error_t::success);
                return true;
            }
            case test_abi::v1::control_operation::acceptance_worker_tick: {
                const cpu_id_t cpu = arch::cpu::current_id();
                if (cpu >= thread::maximum_cpu_count || current.owner == nullptr ||
                    current.owner->root) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                const word_t failure = arch::syscall::argument(frame, 1U);
                const u64 operations = __atomic_add_fetch(&thread::certification_operations[cpu],
                                                          1U, __ATOMIC_RELAXED);
                if (failure != 0U) {
                    __atomic_fetch_add(&thread::certification_failures[cpu], failure,
                                       __ATOMIC_RELAXED);
                }
                if (operations == 4096U) {
                    pr_info("root fuzz cpu=%u operations=%llu failures=%llu status=PASS\n",
                            static_cast<unsigned int>(cpu),
                            static_cast<unsigned long long>(operations),
                            static_cast<unsigned long long>(__atomic_load_n(
                                &thread::certification_failures[cpu], __ATOMIC_ACQUIRE)));
                }
                set_control_result(frame, error_t::success);
                return true;
            }
            case test_abi::v1::control_operation::memory_inject_extent_failure: {
                if (current.owner == nullptr || !current.owner->root) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                verification::configure_extent_node_failure(
                    static_cast<u32>(arch::syscall::argument(frame, 1U)));
                set_control_result(frame, error_t::success);
                return true;
            }
            case test_abi::v1::control_operation::memory_invariant_snapshot: {
                if (current.owner == nullptr || !current.owner->root) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                const auto snapshot = memory::snapshot_invariants();
                const u64 signature = static_cast<u64>(snapshot.free_physical_pages) ^
                                      (static_cast<u64>(snapshot.extent_nodes_in_use) << 32U) ^
                                      (static_cast<u64>(snapshot.resources_in_use) << 40U) ^
                                      (static_cast<u64>(snapshot.frames_in_use) << 48U) ^
                                      (static_cast<u64>(snapshot.page_tables_in_use) << 56U) ^
                                      (snapshot.resource_used_pages * 0x9e3779b97f4a7c15ULL) ^
                                      (snapshot.resource_delegated_pages * 0xbf58476d1ce4e5b9ULL);
                frame.x[1] = signature;
                set_control_result(frame, memory::invariants_valid() ? error_t::success
                                                                     : error_t::invalid_argument);
                return true;
            }
            case test_abi::v1::control_operation::acceptance_query: {
                const word_t cpu = arch::syscall::argument(frame, 1U);
                const word_t selector = arch::syscall::argument(frame, 2U);
                if (current.owner == nullptr || !current.owner->root ||
                    cpu >= thread::maximum_cpu_count) {
                    set_control_result(frame, error_t::denied);
                    return true;
                }
                const u64 value =
                    selector == 0U
                        ? __atomic_load_n(&thread::certification_operations[cpu], __ATOMIC_ACQUIRE)
                        : __atomic_load_n(&thread::certification_failures[cpu], __ATOMIC_ACQUIRE);
                arch::syscall::set_result(frame, static_cast<word_t>(value));
                return true;
            }
            case test_abi::v1::control_operation::hypervisor_self_test:
#if CONFIG_HYPERVISOR_SELFTEST
                if (current.owner == nullptr || !current.owner->root)
                    set_control_result(frame, error_t::denied);
                else
                    set_control_result(frame, hypervisor::test::run_all());
#else
                set_control_result(frame, error_t::unsupported);
#endif
                return true;
        }
#endif
        const auto operation = static_cast<abi::v1::control_operation>(raw_operation);
        const word_t a1 = arch::syscall::argument(frame, 1U);
        const word_t a2 = arch::syscall::argument(frame, 2U);
        const word_t a3 = arch::syscall::argument(frame, 3U);
        const word_t a4 = arch::syscall::argument(frame, 4U);
        const word_t a5 = arch::syscall::argument(frame, 5U);
        error_t result = error_t::unsupported;

        switch (operation) {
            case abi::v1::control_operation::capability_copy:
            case abi::v1::control_operation::capability_mint:
            case abi::v1::control_operation::capability_move:
            case abi::v1::control_operation::capability_delete:
            case abi::v1::control_operation::capability_revoke:
                result = dispatch_capability_operation(current, operation, a1, a2, a3,
                                                       static_cast<u32>(a4),
                                                       static_cast<capability::badge_t>(a5));
                break;
            case abi::v1::control_operation::thread_start:
            case abi::v1::control_operation::thread_resume: {
                thread::thread* target = nullptr;
                result = resolve_thread(current, a1, capability::right_t::control, target);
                if (result == error_t::success) {
                    thread::store_state(*target, thread::state::ready);
                    result = error_t::success;
                }
                break;
            }
            case abi::v1::control_operation::thread_exit: {
                /*
                 * Returning from a userspace program is a one-way transition.
                 * Optional a2/a3 arguments provide an atomic exit notification:
                 * publish the terminal state first, signal the supervisor, then
                 * commit another user context or the permanent kernel-idle root.
                 * A completion badge can therefore never race a later exit
                 * syscall or userspace instruction stream.
                 */
                notification::notification* exit_notification = nullptr;
                if (a2 != 0U) {
                    if (current.owner == nullptr) {
                        result = error_t::denied;
                        break;
                    }
                    object::header_t* notification_header = nullptr;
                    result =
                        capability::lookup(current.owner->cspace, a2, object::type_t::notification,
                                           capability::right_t::write, notification_header);
                    if (result != error_t::success)
                        break;
                    exit_notification =
                        reinterpret_cast<notification::notification*>(notification_header);
                }

                thread::lock_ipc_lifecycle();
                if (current.reply.valid && current.reply.caller < thread::user_thread_count) {
                    thread::thread& caller = thread::user_threads[current.reply.caller];
                    const thread::state caller_state = thread::load_state(caller);
                    if (caller.object.generation == current.reply.generation) {
                        if (caller_state == thread::state::blocked_reply) {
                            caller.ipc_timeout_active = false;
                            thread::clear_transfer(caller.transfer);
                            caller.pending_result = error_t::timed_out;
                            (void)thread::wake(caller);
                            ipc::remote_reschedule(caller.pinned_cpu, arch::cpu::current_id());
                        } else if (caller_state == thread::state::blocked_fault) {
                            caller.ipc_timeout_active = false;
                            caller.waiting_endpoint = 0U;
                            caller.fault_disposition = fault::disposition::terminate;
                            thread::store_state(caller, thread::state::terminated);
                        }
                    }
                    if (current.reply.donation_active)
                        scheduling::revoke_donation(current.scheduling_context,
                                                    caller.scheduling_context);
                    current.reply = {};
                }
                thread::prepare_block(frame, thread::state::terminated);
                if (exit_notification != nullptr)
                    notification::signal(*exit_notification, a3);
                thread::unlock_ipc_lifecycle();
                thread::schedule_prepared(frame);
                return true;
            }
            case abi::v1::control_operation::thread_suspend: {
                thread::thread* target = nullptr;
                result = resolve_thread(current, a1, capability::right_t::control, target);
                if (result == error_t::success)
                    result = thread::quiesce_user_thread(*target);
                break;
            }
            case abi::v1::control_operation::map_frame: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                capability::lock_authority();
                capability::slot_t space_cap{};
                capability::slot_t frame_cap{};
                result = capability::lookup_slot(current.owner->cspace, a1,
                                                 object::type_t::address_space,
                                                 capability::right_t::control, space_cap);
                if (result == error_t::success)
                    result =
                        capability::lookup_slot(current.owner->cspace, a2, object::type_t::frame,
                                                capability::right_t::write, frame_cap);
                if (result == error_t::success) {
                    auto* target_space =
                        reinterpret_cast<space::address_space*>(object::resolve(space_cap.object));
                    auto* source_frame =
                        reinterpret_cast<memory::frame*>(object::resolve(frame_cap.object));
                    if (target_space == nullptr || source_frame == nullptr)
                        result = error_t::not_found;
                    else
                        result =
                            memory::map(*target_space, *source_frame, a3,
                                        static_cast<memory::permission>(a4), frame_cap.derivation,
                                        space_cap.derivation, memory::decode_attributes(a5));
                }
                capability::unlock_authority();
                break;
            }
            case abi::v1::control_operation::frame_create: {
                task::task* target_task = current.owner;
                if (a1 != 0U)
                    result = resolve_task(current, a1, capability::right_t::control, target_task);
                else
                    result = target_task != nullptr ? error_t::success : error_t::denied;
                if (result == error_t::success)
                    result = memory::create_frame(*target_task, static_cast<capability_id_t>(a2));
                break;
            }
            case abi::v1::control_operation::device_frame_create:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result = memory::create_device_frame(*current.owner,
                                                         static_cast<capability_id_t>(a1), a2);
                break;
            case abi::v1::control_operation::memory_resource_delegate: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                task::task* target_task = nullptr;
                result = resolve_task(current, a1, capability::right_t::control, target_task);
                memory::resource* parent = nullptr;
                if (result == error_t::success)
                    result = memory::resolve_resource(*current.owner,
                                                      static_cast<capability_id_t>(a2), parent);
                if (result == error_t::success)
                    result = memory::delegate_resource(*current.owner, *parent, *target_task,
                                                       static_cast<capability_id_t>(a3),
                                                       static_cast<u32>(a4));
                break;
            }
            case abi::v1::control_operation::resource_frame_create: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                memory::resource* authority = nullptr;
                result = memory::resolve_resource(*current.owner, static_cast<capability_id_t>(a1),
                                                  authority);
                if (result == error_t::success)
                    result = memory::create_frame_from_resource(*current.owner, *authority,
                                                                static_cast<capability_id_t>(a2));
                break;
            }
            case abi::v1::control_operation::resource_page_table_create: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                memory::resource* authority = nullptr;
                result = memory::resolve_resource(*current.owner, static_cast<capability_id_t>(a1),
                                                  authority);
                if (result == error_t::success)
                    result = memory::create_page_table_from_resource(
                        *current.owner, *authority, static_cast<capability_id_t>(a2),
                        static_cast<u8>(a3));
                break;
            }
            case abi::v1::control_operation::memory_resource_destroy:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result =
                        memory::destroy_resource(*current.owner, static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::memory_resource_query: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                memory::resource* authority = nullptr;
                result = memory::resolve_resource(*current.owner, static_cast<capability_id_t>(a1),
                                                  authority);
                if (result == error_t::success) {
                    frame.x[1] = __atomic_load_n(&authority->used_pages, __ATOMIC_ACQUIRE);
                    frame.x[2] = authority->quota_pages;
                    frame.x[3] = __atomic_load_n(&authority->delegated_pages, __ATOMIC_ACQUIRE);
                }
                break;
            }
            case abi::v1::control_operation::frame_destroy:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result =
                        memory::destroy_frame(*current.owner, static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::page_table_create: {
                task::task* target_task = current.owner;
                if (a1 != 0U)
                    result = resolve_task(current, a1, capability::right_t::control, target_task);
                else
                    result = target_task != nullptr ? error_t::success : error_t::denied;
                if (result == error_t::success)
                    result = memory::create_page_table(
                        *target_task, static_cast<capability_id_t>(a2), static_cast<u8>(a3));
                break;
            }
            case abi::v1::control_operation::page_table_destroy:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result = memory::destroy_page_table(*current.owner,
                                                        static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::memory_set_quota: {
                if (current.owner == nullptr || !current.owner->root) {
                    result = error_t::denied;
                    break;
                }
                task::task* target_task = nullptr;
                result = resolve_task(current, a1, capability::right_t::control, target_task);
                if (result == error_t::success) {
                    if (a2 == 0U || a2 > memory::managed_pages)
                        result = error_t::invalid_argument;
                    else if (target_task->memory_pages_owned > a2)
                        result = error_t::busy;
                    else {
                        target_task->memory_quota_pages = static_cast<u32>(a2);
                        result = error_t::success;
                    }
                }
                break;
            }
            case abi::v1::control_operation::memory_query:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else {
                    frame.x[1] = current.owner->memory_pages_owned;
                    frame.x[2] = current.owner->memory_quota_pages;
                    frame.x[3] = memory::free_pages;
                    frame.x[4] = memory::managed_pages;
                    result = error_t::success;
                }
                break;
            case abi::v1::control_operation::fault_reply_map: {
                thread::thread* target = nullptr;
                memory::frame* source = nullptr;
                result = resolve_thread(current, a1, capability::right_t::control, target);
                if (result == error_t::success)
                    result = resolve_frame(current, a2, capability::right_t::write, source);
                if (result == error_t::success) {
                    result = thread::resolve_fault_with_frame(current, *target, *source, a3,
                                                              static_cast<memory::permission>(a4));
                }
                break;
            }
            case abi::v1::control_operation::fault_reply_sender: {
                memory::frame* source = nullptr;
                result = resolve_frame(current, a1, capability::right_t::write, source);
                if (result != error_t::success)
                    break;
                if (!current.reply.valid) {
                    result = error_t::invalid_argument;
                    break;
                }
                if (current.reply.caller >= thread::user_thread_count) {
                    current.reply = {};
                    result = error_t::not_found;
                    break;
                }
                auto& caller = thread::user_threads[current.reply.caller];
                if (caller.object.type != object::type_t::thread ||
                    caller.object.generation != current.reply.generation) {
                    current.reply = {};
                    result = error_t::not_found;
                    break;
                }
                result = thread::resolve_fault_with_frame(current, caller, *source, a2,
                                                          static_cast<memory::permission>(a3));
                if (result == error_t::success)
                    current.reply = {};
                break;
            }
            case abi::v1::control_operation::pager_reclaim_sender: {
                memory::frame* source = nullptr;
                result = resolve_frame(current, a1, capability::right_t::write, source);
                if (result != error_t::success)
                    break;
                if (!current.reply.valid) {
                    result = error_t::invalid_argument;
                    break;
                }
                if (current.reply.caller >= thread::user_thread_count) {
                    current.reply = {};
                    result = error_t::not_found;
                    break;
                }
                auto& caller = thread::user_threads[current.reply.caller];
                if (caller.object.type != object::type_t::thread ||
                    caller.object.generation != current.reply.generation) {
                    current.reply = {};
                    result = error_t::not_found;
                    break;
                }
                result = memory::unmap(caller.address_space, *source, a2);
                if (result == error_t::success && current.owner != nullptr) {
                    result =
                        memory::destroy_frame(*current.owner, static_cast<capability_id_t>(a1));
                }
                break;
            }
            case abi::v1::control_operation::frame_allocate: {
                const capability::authority_guard authority_transaction{};
                memory::frame* target_frame = nullptr;
                result = resolve_frame(current, a1, capability::right_t::control, target_frame);
                if (result == error_t::success) {
                    result = memory::assign_frame(
                        *target_frame,
                        current.owner == nullptr ? 0U : current.owner->address_space_id);
                }
                break;
            }
            case abi::v1::control_operation::frame_release: {
                const capability::authority_guard authority_transaction{};
                memory::frame* target_frame = nullptr;
                result = resolve_frame(current, a1, capability::right_t::control, target_frame);
                if (result == error_t::success)
                    result = memory::release_frame(*target_frame);
                break;
            }
            case abi::v1::control_operation::unmap_frame: {
                space::address_space* target_space = nullptr;
                memory::frame* source_frame = nullptr;
                capability::lock_authority();
                result = resolve_space(current, a1, target_space);
                if (result == error_t::success)
                    result = resolve_frame(current, a2, capability::right_t::write, source_frame);
                if (result == error_t::success)
                    result = memory::unmap(*target_space, *source_frame, a3);
                capability::unlock_authority();
                break;
            }
            case abi::v1::control_operation::endpoint_create:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result = ipc::create(*current.owner, static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::endpoint_destroy:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result = ipc::destroy(*current.owner, static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::notification_create:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result = notification::create(*current.owner, static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::notification_destroy:
                if (current.owner == nullptr)
                    result = error_t::denied;
                else
                    result =
                        notification::destroy(*current.owner, static_cast<capability_id_t>(a1));
                break;
            case abi::v1::control_operation::notification_signal:
            case abi::v1::control_operation::notification_poll: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                object::header_t* header = nullptr;
                result =
                    capability::lookup(current.owner->cspace, a1, object::type_t::notification,
                                       operation == abi::v1::control_operation::notification_signal
                                           ? capability::right_t::write
                                           : capability::right_t::read,
                                       header);
                if (result == error_t::success) {
                    auto& notification = *reinterpret_cast<notification::notification*>(header);
                    if (operation == abi::v1::control_operation::notification_signal) {
                        notification::signal(notification, a2);
                    } else {
                        frame.x[1] = notification::consume(notification);
                    }
                }
                break;
            }
            case abi::v1::control_operation::interrupt_bind: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                object::header_t* interrupt_header = nullptr;
                object::header_t* notification_header = nullptr;
                result = capability::lookup(current.owner->cspace, a1, object::type_t::interrupt,
                                            capability::right_t::control, interrupt_header);
                if (result == error_t::success) {
                    result =
                        capability::lookup(current.owner->cspace, a2, object::type_t::notification,
                                           capability::right_t::write, notification_header);
                }
                if (result == error_t::success) {
                    auto& interrupt = *reinterpret_cast<interrupt::interrupt_t*>(interrupt_header);
                    auto& target =
                        *reinterpret_cast<notification::notification*>(notification_header);
                    result = interrupt::bind(interrupt, object::reference(target.object));
                }
                break;
            }
            case abi::v1::control_operation::interrupt_ack: {
                if (current.owner == nullptr) {
                    result = error_t::denied;
                    break;
                }
                object::header_t* interrupt_header = nullptr;
                result = capability::lookup(current.owner->cspace, a1, object::type_t::interrupt,
                                            capability::right_t::write, interrupt_header);
                if (result == error_t::success) {
                    auto& interrupt = *reinterpret_cast<interrupt::interrupt_t*>(interrupt_header);
                    result = interrupt::acknowledge(interrupt);
                }
                break;
            }
            case abi::v1::control_operation::scheduling_configure: {
                thread::thread* target = nullptr;
                result = resolve_thread(current, a1, capability::right_t::control, target);
                if (result == error_t::success) {
                    const cpu_id_t affinity =
                        a5 == 0U ? target->pinned_cpu : static_cast<cpu_id_t>(a5 - 1U);
                    if (a2 > scheduling::highest_priority ||
                        affinity >= thread::maximum_cpu_count) {
                        result = error_t::invalid_argument;
                        break;
                    }
                    /*
                     * Configuration resets budget, replenishment, and
                     * donation state as one lifecycle transition. Requiring
                     * explicit suspension prevents a remote scheduler tick
                     * from observing a partially rewritten context.
                     */
                    thread::lock_ipc_lifecycle();
                    if (thread::load_state(*target) != thread::state::suspended ||
                        target->reply.valid || target->scheduling_context.donation_depth != 0U ||
                        target->scheduling_context.donated_ticks != 0U) {
                        result = error_t::busy;
                    } else {
                        result = scheduling::configure(target->scheduling_context,
                                                       static_cast<u8>(a2), a3, a4, affinity,
                                                       platform::timer::ticks(affinity));
                        if (result == error_t::success)
                            target->pinned_cpu = affinity;
                    }
                    thread::unlock_ipc_lifecycle();
                }
                break;
            }
            case abi::v1::control_operation::child_create:
            case abi::v1::control_operation::process_create:
                if (current.owner == nullptr || !current.owner->root) {
                    result = error_t::denied;
                    break;
                }
                result = thread::create_user_bundle(
                    *current.owner, static_cast<cpu_id_t>(a1), a2, static_cast<capability_id_t>(a3),
                    static_cast<capability_id_t>(a4), static_cast<capability_id_t>(a5));
                if (result == error_t::success) {
                    platform::interrupt::send_ipi_all_others(platform::interrupt::reschedule_ipi);
                }
                break;
            case abi::v1::control_operation::child_destroy:
            case abi::v1::control_operation::process_destroy:
                if (current.owner == nullptr || !current.owner->root) {
                    result = error_t::denied;
                    break;
                }
                result = thread::destroy_user_bundle(
                    *current.owner, static_cast<capability_id_t>(a1),
                    static_cast<capability_id_t>(a2), static_cast<capability_id_t>(a3));
                break;
            case abi::v1::control_operation::hypervisor_invoke: {
                const auto hv_operation = static_cast<abi::v1::hypervisor_operation>(a1);
                hypervisor::virtual_machine_t* vm = nullptr;
                hypervisor::virtual_cpu_t* vcpu = nullptr;
                switch (hv_operation) {
                    case abi::v1::hypervisor_operation::vm_reset:
                        result = resolve_vm(current, a2, capability::right_t::control, vm);
                        if (result == error_t::success)
                            result = hypervisor::reset(*vm);
                        break;
                    case abi::v1::hypervisor_operation::stage2_map:
                        result = resolve_vm(current, a2, capability::right_t::control, vm);
                        if (result == error_t::success)
                            result = hypervisor::stage2_map(*vm, a3, a4, hypervisor::page_size,
                                                            static_cast<u32>(a5));
                        break;
                    case abi::v1::hypervisor_operation::stage2_unmap:
                        result = resolve_vm(current, a2, capability::right_t::control, vm);
                        if (result == error_t::success)
                            result = hypervisor::stage2_unmap(*vm, a3);
                        break;
                    case abi::v1::hypervisor_operation::vcpu_configure:
                        result = resolve_vcpu(current, a2, capability::right_t::control, vcpu);
                        if (result == error_t::success)
                            result = hypervisor::configure_vcpu(*vcpu, a3, a4, a5);
                        break;
                    case abi::v1::hypervisor_operation::virtual_irq_inject:
                        result = resolve_vcpu(current, a2, capability::right_t::control, vcpu);
                        if (result == error_t::success)
                            result = hypervisor::inject_irq(*vcpu, static_cast<u16>(a3));
                        break;
                    case abi::v1::hypervisor_operation::vcpu_suspend:
                        result = resolve_vcpu(current, a2, capability::right_t::control, vcpu);
                        if (result == error_t::success) {
                            if (vcpu->running)
                                result = error_t::busy;
                            else {
                                vcpu->state = hypervisor::vm_state::stopped;
                                result = error_t::success;
                            }
                        }
                        break;
                    case abi::v1::hypervisor_operation::vcpu_run: {
                        result = resolve_vcpu(current, a2, capability::right_t::execute, vcpu);
                        if (result == error_t::success) {
                            hypervisor::exit_record exit{};
                            result = hypervisor::run(*vcpu, exit);
                            frame.x[1] = static_cast<word_t>(exit.reason);
                            frame.x[2] = exit.syndrome;
                            frame.x[3] = exit.fault_address;
                            frame.x[4] = exit.guest_pc;
                        }
                        break;
                    }
                    case abi::v1::hypervisor_operation::diagnostics:
                        result = resolve_vm(current, a2, capability::right_t::read, vm);
                        if (result == error_t::success) {
                            frame.x[1] = vm->last_diagnostic.checkpoint;
                            frame.x[2] =
                                static_cast<word_t>(static_cast<s64>(vm->last_diagnostic.result));
                            frame.x[3] = vm->last_diagnostic.ipa;
                            frame.x[4] = vm->last_diagnostic.value;
                        }
                        break;
                }
                break;
            }
        }
        set_control_result(frame, result);
        return true;
    }
} // namespace sys::kernel::syscall
