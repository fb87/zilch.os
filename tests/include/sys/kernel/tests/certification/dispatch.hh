#pragma once

#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/hypervisor.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/syscall/result.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>
#include <abi/sys/v1/syscall_numbers.hh>
/*
 * Gated on __aarch64__, not just CONFIG_HYPERVISOR_SELFTEST: control_models.hh
 * is written against arm64-specific arch::hypervisor internals (guest system
 * register encodings, HVC ABI) that amd64's stub hypervisor doesn't share.
 * The hypervisor_self_test case below calls none of it on amd64.
 */
#if CONFIG_HYPERVISOR_SELFTEST && defined(__aarch64__)
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
#include <sys/test_abi/v1/certification.hh>
#include <sys/types.hh>

/*
 * The kernel-side half of the userspace certification harness's syscall
 * surface (test_abi::v1::control_operation) -- exercised only by the
 * legacy CONFIG_SELFTEST build's ledger-based self-test main() (see
 * src/user/init/main.cc), never by production boot. Extracted out of
 * dispatch_control() in sys/kernel/syscall/control.hh, which now just
 * calls dispatch() here before falling through to real control operations.
 */
namespace sys::kernel::tests::certification
{
    [[nodiscard]] inline bool process_lifecycle_valid() noexcept {
        /*
         * owner is followed via value.owner rather than indexed as
         * thread::user_tasks[index]: create_user_bundle() always allocates
         * a task+thread pair at the same array index, but
         * create_user_thread() (thread_create, an additional thread
         * sharing an EXISTING task's cspace/authority rather than a fresh
         * one) legitimately has value.owner pointing at a task:task living
         * at a DIFFERENT index -- the paired user_tasks[index] at the
         * thread's own index stays default/unused in that case. Following
         * the real pointer instead of assuming same-index pairing keeps
         * this check correct for both cases.
         */
        for (u32 index = 0U; index < thread::user_thread_count; ++index) {
            const thread::thread& value = thread::user_threads[index];
            const thread::state current = thread::load_state(value);
            if (value.object.type == kernel::object::type_t::none) {
                if (current != thread::state::inactive || value.owner != nullptr ||
                    value.address_space.object.type != kernel::object::type_t::none ||
                    value.scheduling_context.object.type != kernel::object::type_t::none)
                    return false;
                continue;
            }
            if (value.object.type != kernel::object::type_t::thread || value.owner == nullptr ||
                value.owner->object.type != kernel::object::type_t::task ||
                value.address_space.object.type != kernel::object::type_t::address_space ||
                value.scheduling_context.object.type !=
                    kernel::object::type_t::scheduling_context ||
                value.pinned_cpu >= thread::maximum_cpu_count ||
                value.scheduling_context.affinity >= thread::maximum_cpu_count ||
                value.scheduling_context.donation_depth >
                    kernel::scheduling::maximum_donation_depth ||
                value.scheduling_context.replenishment_count >
                    kernel::scheduling::maximum_replenishments ||
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
            if (value.object.type == kernel::object::type_t::none)
                continue;
            const kernel::scheduling::context& context = value.scheduling_context;
            if (value.pinned_cpu >= thread::maximum_cpu_count ||
                context.affinity != value.pinned_cpu || context.budget_ticks == 0U ||
                context.period_ticks == 0U || context.budget_ticks > context.period_ticks ||
                context.effective_priority > context.maximum_priority ||
                context.donation_depth > kernel::scheduling::maximum_donation_depth ||
                context.replenishment_count > kernel::scheduling::maximum_replenishments ||
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
                 context.next_replenishment != kernel::scheduling::maximum_time))
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

    [[nodiscard]] inline bool dispatch(thread::thread& current, arch::thread::context& frame,
                                       word_t raw_operation) noexcept {
        const auto test_operation = static_cast<test_abi::v1::control_operation>(raw_operation);
        switch (test_operation) {
            case test_abi::v1::control_operation::acceptance_report: {
                if (current.owner == nullptr || !current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
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
                    case 35U:
                        name = "userspace_control_plane_graph";
                        break;
                    case 36U:
                        name = "domain_manager_api";
                        break;
                    case 37U:
                        name = "hypervisor_dynamic_lifecycle";
                        break;
                    case 38U:
                        name = "hypervisor_vm_create";
                        break;
                    case 39U:
                        name = "hypervisor_vcpu_create";
                        break;
                    case 40U:
                        name = "hypervisor_vm_parent_busy";
                        break;
                    case 41U:
                        name = "hypervisor_vcpu_destroy";
                        break;
                    case 42U:
                        name = "hypervisor_vcpu_stale";
                        break;
                    case 43U:
                        name = "hypervisor_vm_destroy";
                        break;
                    case 44U:
                        name = "hypervisor_vm_stale";
                        break;
                    case 45U:
                        name = "hypervisor_vm_reuse";
                        break;
                    case 46U:
                        name = "domain_guest_load";
                        break;
                    case 47U:
                        name = "domain_guest_run";
                        break;
                    default:
                        break;
                }
                pr_info("[TEST] name=%s result=%s\n", name, passed != 0U ? "PASS" : "FAIL");
                syscall::set_control_result(frame, passed != 0U ? error_t::success
                                                                : error_t::invalid_argument);
                return true;
            }
            case test_abi::v1::control_operation::acceptance_finalize: {
                if (current.owner == nullptr || !current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
                    return true;
                }
                const word_t passed = arch::syscall::argument(frame, 1U);
                const word_t failures = arch::syscall::argument(frame, 2U);
                const word_t failure_mask = arch::syscall::argument(frame, 3U);
                const word_t transport_ok = arch::syscall::argument(frame, 4U);
                const bool mappings_valid = memory::mapping_database_valid();
                const bool objects_valid = kernel::object::accounting_valid();
                const bool locks_valid = lock_order::violation_count() == 0U;
                const bool endpoints_valid = kernel::ipc::database_valid();
                const bool notifications_valid = notification::database_valid();
                const bool interrupts_valid = kernel::interrupt::database_valid();
                const bool processes_valid = process_lifecycle_valid();
                const bool capabilities_valid = kernel::capability::database_valid();
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
                const u64 ipc_latency_samples = kernel::interrupt::timing::latency_sample_count(
                    kernel::interrupt::timing::latency_kind::ipc_service);
                const u64 ipc_latency_max = kernel::interrupt::timing::latency_max(
                    kernel::interrupt::timing::latency_kind::ipc_service);
                const bool ipc_timing_valid =
                    ipc_latency_samples != 0U &&
                    ipc_latency_max <= kernel::interrupt::timing::latency_target_ticks();
                const bool scheduler_timing_valid =
                    kernel::interrupt::timing::within_target() &&
                    kernel::interrupt::timing::latency_within_target(
                        kernel::interrupt::timing::latency_kind::interrupt_service) &&
                    kernel::interrupt::timing::latency_within_target(
                        kernel::interrupt::timing::latency_kind::preemption_service) &&
                    kernel::interrupt::timing::latency_within_target(
                        kernel::interrupt::timing::latency_kind::cross_cpu_wake) &&
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
                        static_cast<unsigned long long>(
                            kernel::interrupt::timing::latency_target_ticks()));
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
                        static_cast<unsigned long long>(
                            kernel::interrupt::timing::latency_target_ticks()),
                        static_cast<unsigned long long>(ipc_latency_samples));
                pr_info("[TEST] name=memory_inventory_invariants result=%s source=%u regions=%u "
                        "pages=%u\n",
                        memory_inventory_valid ? "PASS" : "FAIL",
                        static_cast<unsigned int>(memory::physical_inventory_source),
                        static_cast<unsigned int>(memory::physical_region_count),
                        static_cast<unsigned int>(memory::managed_pages));
                pr_info("[METRIC] name=irq_disabled_duration_final max_ticks=%llu "
                        "reference_ticks=%llu samples=%llu\n",
                        static_cast<unsigned long long>(kernel::interrupt::timing::maximum()),
                        static_cast<unsigned long long>(kernel::interrupt::timing::target_ticks()),
                        static_cast<unsigned long long>(kernel::interrupt::timing::sample_count()));
                pr_info(
                    "[METRIC] name=interrupt_latency max_ticks=%llu reference_ticks=%llu "
                    "samples=%llu\n",
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_max(
                        kernel::interrupt::timing::latency_kind::interrupt_service)),
                    static_cast<unsigned long long>(
                        kernel::interrupt::timing::latency_target_ticks()),
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_sample_count(
                        kernel::interrupt::timing::latency_kind::interrupt_service)));
                pr_info(
                    "[METRIC] name=preemption_latency max_ticks=%llu reference_ticks=%llu "
                    "samples=%llu\n",
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_max(
                        kernel::interrupt::timing::latency_kind::preemption_service)),
                    static_cast<unsigned long long>(
                        kernel::interrupt::timing::latency_target_ticks()),
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_sample_count(
                        kernel::interrupt::timing::latency_kind::preemption_service)));
                pr_info(
                    "[METRIC] name=cross_cpu_wake_latency max_ticks=%llu "
                    "reference_ticks=%llu samples=%llu\n",
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_max(
                        kernel::interrupt::timing::latency_kind::cross_cpu_wake)),
                    static_cast<unsigned long long>(
                        kernel::interrupt::timing::latency_target_ticks()),
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_sample_count(
                        kernel::interrupt::timing::latency_kind::cross_cpu_wake)));
                pr_info(
                    "[METRIC] name=ipc_latency max_ticks=%llu reference_ticks=%llu "
                    "samples=%llu\n",
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_max(
                        kernel::interrupt::timing::latency_kind::ipc_service)),
                    static_cast<unsigned long long>(
                        kernel::interrupt::timing::latency_target_ticks()),
                    static_cast<unsigned long long>(kernel::interrupt::timing::latency_sample_count(
                        kernel::interrupt::timing::latency_kind::ipc_service)));
                pr_info("[ACCEPTANCE] suite=root-only boot=root-only result=%s failures=%llu "
                        "failure_mask=%llx transport=%s\n",
                        acceptance_passed ? "PASS" : "FAIL",
                        static_cast<unsigned long long>(failures),
                        static_cast<unsigned long long>(failure_mask),
                        transport_ok != 0U ? "PASS" : "FAIL");
                syscall::set_control_result(frame, acceptance_passed ? error_t::success
                                                                     : error_t::invalid_argument);
                return true;
            }
            case test_abi::v1::control_operation::memory_server_protocol_detail: {
                if (current.owner == nullptr || !current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
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
                syscall::set_control_result(frame, error_t::success);
                return true;
            }
            case test_abi::v1::control_operation::acceptance_worker_tick: {
                const cpu_id_t cpu = arch::cpu::current_id();
                if (cpu >= thread::maximum_cpu_count || current.owner == nullptr ||
                    current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
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
                syscall::set_control_result(frame, error_t::success);
                return true;
            }
            case test_abi::v1::control_operation::memory_inject_extent_failure: {
                if (current.owner == nullptr || !current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
                    return true;
                }
                verification::configure_extent_node_failure(
                    static_cast<u32>(arch::syscall::argument(frame, 1U)));
                syscall::set_control_result(frame, error_t::success);
                return true;
            }
            case test_abi::v1::control_operation::memory_invariant_snapshot: {
                if (current.owner == nullptr || !current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
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
                arch::syscall::set_output(frame, 1U, signature);
                syscall::set_control_result(frame, memory::invariants_valid()
                                                       ? error_t::success
                                                       : error_t::invalid_argument);
                return true;
            }
            case test_abi::v1::control_operation::acceptance_query: {
                const word_t cpu = arch::syscall::argument(frame, 1U);
                const word_t selector = arch::syscall::argument(frame, 2U);
                if (current.owner == nullptr || !current.owner->root ||
                    cpu >= thread::maximum_cpu_count) {
                    syscall::set_control_result(frame, error_t::denied);
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
#if CONFIG_HYPERVISOR_SELFTEST && defined(__aarch64__)
                if (current.owner == nullptr || !current.owner->root)
                    syscall::set_control_result(frame, error_t::denied);
                else
                    syscall::set_control_result(frame,
                                                kernel::hypervisor::test::run_runtime_acceptance());
#else
                syscall::set_control_result(frame, error_t::unsupported);
#endif
                return true;
            case test_abi::v1::control_operation::domain_manager_detail:
                if (current.owner == nullptr || !current.owner->root) {
                    syscall::set_control_result(frame, error_t::denied);
                    return true;
                }
                pr_err("[DOMAIN-LOAD] detail=%llx\n",
                       static_cast<unsigned long long>(arch::syscall::argument(frame, 1U)));
                syscall::set_control_result(frame, error_t::success);
                return true;
        }
        return false;
    }
} // namespace sys::kernel::tests::certification
