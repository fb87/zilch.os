#ifndef CONFIG_SELFTEST
#define CONFIG_SELFTEST 0
#endif

#if CONFIG_SELFTEST
#include <sys/certification.hh>
#include <sys/control.hh>
#include <sys/hypervisor.hh>
#include <sys/ipc.hh>
#include <sys/test_abi/v1/certification.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>

namespace
{
    enum class test_id : sys::word_t {
        root_only_boot = 1U,
        bootinfo_contract = 2U,
        capability_control = 3U,
        notification_control = 4U,
        root_created_objects = 5U,
        root_created_smp_fuzz = 6U,
        object_destroy_reuse = 7U,
        hypervisor_real_single_vcpu = 8U,
        hypervisor_negative_fuzz = 9U,
        hypervisor_control_model_0_4 = 10U,
        userspace_pager_service = 11U,
        dynamic_ipc_objects = 12U,
        memory_resource_lifecycle = 13U,
        memory_mapping_database = 14U,
        memory_authority_revoke = 15U,
        memory_attributes_pressure = 16U,
        memory_resource_delegation = 17U,
        memory_extent_retype = 18U,
        memory_extent_metadata = 19U,
        memory_pressure_rollback = 20U,
        memory_server_protocol = 21U,
        ipc_lifecycle_races = 22U,
        capability_transfer_revoke_race = 23U,
        object_lookup_destroy_race = 24U,
        scheduling_configuration = 25U,
        ipc_capability_batch = 26U,
        ipc_ool_frame_grant = 27U,
        ipc_completion_gate = 28U,
        memory_completion_gate = 29U,
        capability_completion_gate = 30U,
        scheduler_completion_gate = 31U,
    };

    inline constexpr sys::word_t worker_threshold = 4096U;
    inline constexpr sys::word_t worker_thread_selector_base = 17U;
    inline constexpr sys::word_t worker_task_selector_base = 20U;
    inline constexpr sys::word_t worker_space_selector_base = 23U;
    inline constexpr sys::word_t hypervisor_vm_selector = 28U;

    static_assert(worker_space_selector_base + 3U < hypervisor_vm_selector);

    inline constexpr sys::word_t memory_server_role = 0x100U;
    inline constexpr sys::word_t fault_client_role = 0x101U;
    inline constexpr sys::word_t second_fault_client_role = 0x102U;
    inline constexpr sys::word_t memory_client_role_base = 0x103U;
    inline constexpr sys::word_t undefined_instruction_role = 0x106U;
    inline constexpr sys::word_t ipc_lifecycle_client_role_base = 0x110U;
    inline constexpr sys::word_t capability_race_server_role = 0x115U;
    inline constexpr sys::word_t capability_race_sender_role = 0x116U;
    inline constexpr sys::word_t object_race_worker_role = 0x117U;

    [[nodiscard]] bool create_service_process(sys::word_t cpu, sys::word_t role,
                                              sys::word_t thread_selector,
                                              sys::word_t task_selector,
                                              sys::word_t space_selector) noexcept {
        return sys::control(sys::abi::v1::control_operation::process_create, cpu, role,
                            thread_selector, task_selector,
                            space_selector) == static_cast<sys::word_t>(sys::error_t::success);
    }

    [[nodiscard]] bool destroy_service_process(sys::word_t thread_selector,
                                               sys::word_t task_selector,
                                               sys::word_t space_selector,
                                               sys::word_t* failure = nullptr) noexcept {
        const sys::word_t suspend =
            sys::control(sys::abi::v1::control_operation::thread_suspend, thread_selector);
        if (suspend != static_cast<sys::word_t>(sys::error_t::success)) {
            if (failure != nullptr)
                *failure = suspend;
            return false;
        }
        for (sys::word_t attempt = 0U; attempt < 100000U; ++attempt) {
            const sys::word_t result =
                sys::control(sys::abi::v1::control_operation::process_destroy, thread_selector,
                             task_selector, space_selector);
            if (result == static_cast<sys::word_t>(sys::error_t::success)) {
                return true;
            }
            if (result != static_cast<sys::word_t>(sys::error_t::busy)) {
                if (failure != nullptr)
                    *failure = result;
                return false;
            }
        }
        if (failure != nullptr)
            *failure = static_cast<sys::word_t>(sys::error_t::busy);
        return false;
    }

    [[nodiscard]] bool wait_for_badges(sys::word_t expected,
                                       sys::word_t* failure_detail = nullptr) noexcept {
        sys::word_t observed = 0U;
        for (sys::word_t spin = 0U; spin < 100000000U; ++spin) {
            sys::word_t badges = 0U;
            const sys::word_t status = sys::control_result1(
                badges, sys::abi::v1::control_operation::notification_poll, 14U);
            if (status == static_cast<sys::word_t>(sys::error_t::success)) {
                if ((badges & 0xffff0000U) != 0U) {
                    if (failure_detail != nullptr)
                        *failure_detail = badges;
                    return false;
                }
                observed |= badges;
                if ((observed & expected) == expected)
                    return true;
            }
        }
        if (failure_detail != nullptr)
            *failure_detail = 0x10000000U | observed;
        return false;
    }

    struct service_results final {
        bool pager{};
        bool memory_protocol{};
    };

    [[nodiscard]] bool test_ipc_lifecycle_races() noexcept {
        constexpr sys::word_t thread_selector = 55U;
        constexpr sys::word_t task_selector = 56U;
        constexpr sys::word_t space_selector = 57U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        bool passed = create_service_process(1U, ipc_lifecycle_client_role_base, thread_selector,
                                             task_selector, space_selector);
        if (passed)
            passed = wait_for_badges(1U << 6U);
        if (passed) {
            passed = false;
            for (sys::word_t attempt = 0U; attempt < 100000U; ++attempt) {
                const sys::word_t cancelled = sys_ipc_invoke_raw(
                    0U, static_cast<sys::word_t>(sys::abi::v1::ipc_operation::cancel),
                    thread_selector, 0U, 0U, 0U, 0U, 0U);
                if (cancelled == success) {
                    passed = true;
                    break;
                }
                if (cancelled != static_cast<sys::word_t>(sys::error_t::not_found))
                    break;
            }
        }
        if (passed)
            passed = wait_for_badges(1U << 7U);
        passed = destroy_service_process(thread_selector, task_selector, space_selector) && passed;

        if (passed)
            passed = create_service_process(1U, ipc_lifecycle_client_role_base + 1U,
                                            thread_selector, task_selector, space_selector);
        if (passed)
            passed = wait_for_badges(1U << 8U);
        passed = destroy_service_process(thread_selector, task_selector, space_selector) && passed;

        if (passed)
            passed = create_service_process(1U, ipc_lifecycle_client_role_base + 2U,
                                            thread_selector, task_selector, space_selector);
        if (passed)
            passed = wait_for_badges(1U << 9U);
        passed = destroy_service_process(thread_selector, task_selector, space_selector) && passed;

        constexpr sys::word_t caller_thread_selector = 58U;
        constexpr sys::word_t caller_task_selector = 59U;
        constexpr sys::word_t caller_space_selector = 60U;
        if (passed)
            passed = create_service_process(1U, ipc_lifecycle_client_role_base + 3U,
                                            thread_selector, task_selector, space_selector);
        if (passed)
            passed = create_service_process(2U, ipc_lifecycle_client_role_base + 4U,
                                            caller_thread_selector, caller_task_selector,
                                            caller_space_selector);
        if (passed)
            passed = wait_for_badges((1U << 10U) | (1U << 11U));
        passed = destroy_service_process(caller_thread_selector, caller_task_selector,
                                         caller_space_selector) &&
                 passed;
        passed = destroy_service_process(thread_selector, task_selector, space_selector) && passed;
        return passed;
    }

    [[nodiscard]] bool test_capability_transfer_revoke_race() noexcept {
        constexpr sys::word_t server_thread = 55U;
        constexpr sys::word_t server_task = 56U;
        constexpr sys::word_t server_space = 57U;
        constexpr sys::word_t sender_thread = 58U;
        constexpr sys::word_t sender_task = 59U;
        constexpr sys::word_t sender_space = 60U;
        constexpr sys::word_t authority = 30U;
        constexpr sys::word_t rollback_authority = 31U;
        constexpr sys::word_t transferred_slot = 20U;
        constexpr sys::word_t ready_badge = 1U << 12U;
        constexpr sys::word_t sender_started_badge = 1U << 13U;
        constexpr sys::word_t sender_done_badge = 1U << 15U;
        constexpr sys::word_t cap_write = 1U << 1U;
        constexpr sys::word_t cap_grant = 1U << 3U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t not_found = static_cast<sys::word_t>(sys::error_t::not_found);

        bool server_created = false;
        bool sender_created = false;
        bool authority_created = false;
        bool rollback_authority_created = false;
        bool passed = sys::control(sys::abi::v1::control_operation::notification_create,
                                   authority) == success;
        authority_created = passed;
        if (passed) {
            rollback_authority_created =
                sys::control(sys::abi::v1::control_operation::notification_create,
                             rollback_authority) == success;
            passed = rollback_authority_created;
        }
        if (passed) {
            server_created = create_service_process(1U, capability_race_server_role, server_thread,
                                                    server_task, server_space);
            passed = server_created;
        }
        if (passed)
            passed = wait_for_badges(ready_badge);
        if (passed) {
            sender_created = create_service_process(2U, capability_race_sender_role, sender_thread,
                                                    sender_task, sender_space);
            passed = sender_created;
        }
        if (passed) {
            passed =
                sys::control(sys::abi::v1::control_operation::capability_mint, sender_task,
                             transferred_slot, authority, cap_write | cap_grant, 0U) == success;
        }
        if (passed) {
            passed = sys::control(sys::abi::v1::control_operation::capability_mint, sender_task,
                                  transferred_slot + 1U, rollback_authority, cap_write | cap_grant,
                                  0U) == success;
        }
        if (passed) {
            passed =
                sys::control(sys::abi::v1::control_operation::capability_mint, server_task,
                             transferred_slot + 1U, rollback_authority, cap_write, 0U) == success;
        }
        if (passed)
            passed = wait_for_badges(sender_started_badge);
        if (passed) {
            passed = sys::control(sys::abi::v1::control_operation::capability_revoke, 0U, 0U,
                                  authority) == success;
        }
        if (passed)
            passed = wait_for_badges(sender_done_badge);

        /*
         * Both legal linearizations have the same postcondition: revoke after
         * mint removes the receiver descendant; revoke before mint makes the
         * transfer fail.  A successful delete here proves authority escaped.
         */
        if (passed) {
            passed = sys::control(sys::abi::v1::control_operation::capability_delete, server_task,
                                  transferred_slot) == not_found;
        }
        if (passed) {
            passed = sys::control(sys::abi::v1::control_operation::capability_delete, server_task,
                                  transferred_slot + 1U) == success;
        }

        if (sender_created)
            passed = destroy_service_process(sender_thread, sender_task, sender_space) && passed;
        if (server_created)
            passed = destroy_service_process(server_thread, server_task, server_space) && passed;
        if (authority_created) {
            const sys::word_t destroy =
                sys::control(sys::abi::v1::control_operation::notification_destroy, authority);
            passed = destroy == success && passed;
        }
        if (rollback_authority_created) {
            const sys::word_t destroy = sys::control(
                sys::abi::v1::control_operation::notification_destroy, rollback_authority);
            passed = destroy == success && passed;
        }
        return passed;
    }

    [[nodiscard]] bool test_object_lookup_destroy_race() noexcept {
        constexpr sys::word_t worker_thread = 55U;
        constexpr sys::word_t worker_task = 56U;
        constexpr sys::word_t worker_space = 57U;
        constexpr sys::word_t notification = 30U;
        constexpr sys::word_t worker_slot = 20U;
        constexpr sys::word_t started_badge = 1U << 12U;
        constexpr sys::word_t completed_badge = 1U << 13U;
        constexpr sys::word_t cap_write = 1U << 1U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        bool notification_created =
            sys::control(sys::abi::v1::control_operation::notification_create, notification) ==
            success;
        bool worker_created = false;
        bool passed = notification_created;
        if (passed) {
            worker_created = create_service_process(1U, object_race_worker_role, worker_thread,
                                                    worker_task, worker_space);
            passed = worker_created;
        }
        if (passed) {
            passed = sys::control(sys::abi::v1::control_operation::capability_mint, worker_task,
                                  worker_slot, notification, cap_write, 0U) == success;
        }
        if (passed)
            passed = wait_for_badges(started_badge);
        if (passed) {
            passed = sys::control(sys::abi::v1::control_operation::notification_destroy,
                                  notification) == success;
            notification_created = !passed;
        }
        if (passed)
            passed = wait_for_badges(completed_badge);
        if (worker_created)
            passed = destroy_service_process(worker_thread, worker_task, worker_space) && passed;
        if (notification_created) {
            passed = sys::control(sys::abi::v1::control_operation::notification_destroy,
                                  notification) == success &&
                     passed;
        }
        return passed;
    }

    [[nodiscard]] bool test_scheduling_configuration() noexcept {
        constexpr sys::word_t thread_selector = 55U;
        constexpr sys::word_t task_selector = 56U;
        constexpr sys::word_t space_selector = 57U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t invalid = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        const sys::word_t busy = static_cast<sys::word_t>(sys::error_t::busy);

        bool passed = sys::control(sys::abi::v1::control_operation::scheduling_configure, 2U, 128U,
                                   5U, 10U) == busy;
        bool created = create_service_process(1U, ipc_lifecycle_client_role_base + 2U,
                                              thread_selector, task_selector, space_selector);
        passed = created && passed;
        if (passed)
            passed = sys::control(sys::abi::v1::control_operation::thread_suspend,
                                  thread_selector) == success;
        if (passed)
            passed = sys::control(sys::abi::v1::control_operation::scheduling_configure,
                                  thread_selector, 256U, 5U, 10U) == invalid &&
                     sys::control(sys::abi::v1::control_operation::scheduling_configure,
                                  thread_selector, 128U, 0U, 10U) == invalid &&
                     sys::control(sys::abi::v1::control_operation::scheduling_configure,
                                  thread_selector, 128U, 11U, 10U) == invalid &&
                     sys::control(sys::abi::v1::control_operation::scheduling_configure,
                                  thread_selector, 200U, 5U, 10U, 6U) == invalid &&
                     sys::control(sys::abi::v1::control_operation::scheduling_configure,
                                  thread_selector, 200U, 5U, 10U, 3U) == success;
        if (created)
            passed =
                destroy_service_process(thread_selector, task_selector, space_selector) && passed;
        return passed;
    }

    [[nodiscard]] service_results run_userspace_services() noexcept {
        /*
         * Reserve a disjoint root-CSpace range for the memory service graph.
         * The earlier layout overlapped client 2 with the server task/space
         * selectors, making teardown operate on the wrong bundle.
         */
        constexpr sys::word_t server_thread = 40U;
        constexpr sys::word_t server_task = 41U;
        constexpr sys::word_t server_space = 42U;
        constexpr sys::word_t client_thread = 43U;
        constexpr sys::word_t client_task = 46U;
        constexpr sys::word_t client_space = 49U;
        constexpr sys::word_t service_selector_end = client_space + 2U;
        static_assert(server_thread < server_task && server_task < server_space);
        static_assert(server_space < client_thread);
        static_assert(client_thread + 2U < client_task);
        static_assert(client_task + 2U < client_space);
        static_assert(service_selector_end < 54U);

        service_results result{};
        if (!create_service_process(1U, memory_server_role, server_thread, server_task,
                                    server_space))
            return result;

        bool pager =
            create_service_process(2U, fault_client_role, client_thread, client_task, client_space);
        if (pager)
            pager = wait_for_badges(1U);
        if (pager)
            pager = destroy_service_process(client_thread, client_task, client_space);

        if (pager)
            pager = create_service_process(3U, second_fault_client_role, client_thread, client_task,
                                           client_space);
        if (pager)
            pager = wait_for_badges(2U);
        if (pager)
            pager = destroy_service_process(client_thread, client_task, client_space);

        if (pager)
            pager = create_service_process(2U, undefined_instruction_role, client_thread,
                                           client_task, client_space);
        if (pager)
            pager = wait_for_badges(1U << 6U);
        if (pager)
            pager = destroy_service_process(client_thread, client_task, client_space);
        result.pager = pager;

        bool pressure = pager;
        for (sys::word_t index = 0U; index < 3U && pressure; ++index) {
            pressure = create_service_process(index + 1U, memory_client_role_base + index,
                                              client_thread + index, client_task + index,
                                              client_space + index);
        }
        if (pressure) {
            /*
             * The server badge proves it has observed every shutdown and
             * reached its terminal state before any service bundle is
             * reclaimed.
             */
            constexpr sys::word_t all_service_badges =
                (1U << 2U) | (1U << 3U) | (1U << 4U) | (1U << 5U);
            sys::word_t failure_detail = 0U;
            pressure = wait_for_badges(all_service_badges, &failure_detail);
            if (!pressure && failure_detail != 0U) {
                (void)sys::certification::control(
                    sys::test_abi::v1::control_operation::memory_server_protocol_detail,
                    failure_detail);
            }
        }
        for (sys::word_t index = 0U; index < 3U; ++index) {
            sys::word_t teardown_error = 0U;
            const bool destroyed = destroy_service_process(
                client_thread + index, client_task + index, client_space + index, &teardown_error);
            if (!destroyed) {
                const sys::word_t detail = (1ULL << 30U) | ((index & 0x1fU) << 24U) | (9U << 16U) |
                                           (teardown_error & 0xffffU);
                (void)sys::certification::control(
                    sys::test_abi::v1::control_operation::memory_server_protocol_detail, detail);
            }
            pressure = destroyed && pressure;
        }
        result.memory_protocol = pressure;

        const bool server_destroyed =
            destroy_service_process(server_thread, server_task, server_space);
        if (!server_destroyed) {
            constexpr sys::word_t detail = (1ULL << 29U) | 0x42U;
            (void)sys::certification::control(
                sys::test_abi::v1::control_operation::memory_server_protocol_detail, detail);
        }
        result.pager = result.pager && server_destroyed;
        result.memory_protocol = result.memory_protocol && server_destroyed;
        return result;
    }

    [[nodiscard]] bool test_dynamic_ipc_objects() noexcept {
        constexpr sys::word_t endpoint_selector = 30U;
        constexpr sys::word_t notification_selector = 31U;
        const sys::word_t endpoint_created =
            sys::control(sys::abi::v1::control_operation::endpoint_create, endpoint_selector);
        const sys::word_t endpoint_busy =
            sys::control(sys::abi::v1::control_operation::endpoint_create, endpoint_selector);
        const sys::word_t endpoint_destroyed =
            sys::control(sys::abi::v1::control_operation::endpoint_destroy, endpoint_selector);
        const sys::word_t endpoint_recreated =
            sys::control(sys::abi::v1::control_operation::endpoint_create, endpoint_selector);
        const sys::word_t endpoint_redestroyed =
            sys::control(sys::abi::v1::control_operation::endpoint_destroy, endpoint_selector);

        const sys::word_t notification_created = sys::control(
            sys::abi::v1::control_operation::notification_create, notification_selector);
        const sys::word_t notification_signalled = sys::control(
            sys::abi::v1::control_operation::notification_signal, notification_selector, 0x55U);
        sys::word_t badges = 0U;
        const sys::word_t notification_polled = sys::control_result1(
            badges, sys::abi::v1::control_operation::notification_poll, notification_selector);
        const sys::word_t notification_destroyed = sys::control(
            sys::abi::v1::control_operation::notification_destroy, notification_selector);
        const sys::word_t notification_recreated = sys::control(
            sys::abi::v1::control_operation::notification_create, notification_selector);
        const sys::word_t notification_redestroyed = sys::control(
            sys::abi::v1::control_operation::notification_destroy, notification_selector);

        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        return endpoint_created == success &&
               endpoint_busy == static_cast<sys::word_t>(sys::error_t::busy) &&
               endpoint_destroyed == success && endpoint_recreated == success &&
               endpoint_redestroyed == success && notification_created == success &&
               notification_signalled == success && notification_polled == success &&
               badges == 0x55U && notification_destroyed == success &&
               notification_recreated == success && notification_redestroyed == success;
    }

    [[nodiscard]] bool test_memory_resource_delegation() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t child_resource_selector = 33U;
        constexpr sys::word_t first_frame = 34U;
        constexpr sys::word_t second_frame = 35U;
        constexpr sys::word_t third_frame = 36U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t no_memory = static_cast<sys::word_t>(sys::error_t::no_memory);

        if (sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                         root_task_selector, root_resource_selector, child_resource_selector,
                         2U) != success)
            return false;
        sys::word_t used = 0U;
        if (sys::control_result1(used, sys::abi::v1::control_operation::memory_resource_query,
                                 child_resource_selector) != success ||
            used != 0U)
            return false;
        bool passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                                   child_resource_selector, first_frame) == success;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource_selector, second_frame) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource_selector, third_frame) == no_memory &&
                 passed;
        if (sys::control_result1(used, sys::abi::v1::control_operation::memory_resource_query,
                                 child_resource_selector) != success ||
            used != 2U)
            passed = false;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, first_frame) == success &&
            passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, second_frame) == success &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              child_resource_selector) == success &&
                 passed;
        return passed;
    }

    [[nodiscard]] bool test_memory_extent_retype() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t parent_resource = 37U;
        constexpr sys::word_t child_resource = 38U;
        constexpr sys::word_t parent_frame0 = 39U;
        constexpr sys::word_t parent_frame1 = 40U;
        constexpr sys::word_t parent_frame2 = 41U;
        constexpr sys::word_t child_frame0 = 42U;
        constexpr sys::word_t child_frame1 = 43U;
        constexpr sys::word_t child_frame2 = 44U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t no_memory = static_cast<sys::word_t>(sys::error_t::no_memory);

        bool passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                   root_task_selector, root_resource_selector, parent_resource,
                                   4U) == success;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                              root_task_selector, parent_resource, child_resource, 2U) == success &&
                 passed;

        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              parent_resource, parent_frame0) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              parent_resource, parent_frame1) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              parent_resource, parent_frame2) == no_memory &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource, child_frame0) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource, child_frame1) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource, child_frame2) == no_memory &&
                 passed;

        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, child_frame0) == success &&
            passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, child_frame1) == success &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              child_resource) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, parent_frame0) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, parent_frame1) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              parent_resource) == success &&
                 passed;
        return passed;
    }

    [[nodiscard]] bool test_memory_extent_metadata() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t parent_resource = 33U;
        constexpr sys::word_t first_child = 34U;
        constexpr sys::word_t child_count = 20U;
        constexpr sys::word_t frame_selector = 54U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        bool passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                   root_task_selector, root_resource_selector, parent_resource,
                                   child_count) == success;
        for (sys::word_t index = 0U; index < child_count && passed; ++index) {
            passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                  root_task_selector, parent_resource, first_child + index,
                                  1U) == success;
        }

        /* Return alternating extents first to force a fragmented parent list. */
        for (sys::word_t parity = 0U; parity < 2U && passed; ++parity) {
            for (sys::word_t index = parity; index < child_count; index += 2U) {
                passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                                      first_child + index) == success &&
                         passed;
            }
        }

        /* A full-size redelegation proves deterministic merge and node reuse. */
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                              root_task_selector, parent_resource, first_child,
                              child_count) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create, first_child,
                              frame_selector) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              first_child) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              parent_resource) == success &&
                 passed;
        return passed;
    }

    [[nodiscard]] bool test_memory_pressure_rollback() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t parent_resource = 33U;
        constexpr sys::word_t child_resource = 34U;
        constexpr sys::word_t first_frame = 35U;
        constexpr sys::word_t frame_count = 16U;
        constexpr sys::word_t overflow_frame = first_frame + frame_count;
        constexpr sys::word_t cycles = 32U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t no_memory = static_cast<sys::word_t>(sys::error_t::no_memory);

        sys::word_t before = 0U;
        if (sys::certification::control_result1(
                before, sys::test_abi::v1::control_operation::memory_invariant_snapshot) != success)
            return false;

        /* Force the split-node allocation to fail and prove full rollback. */
        bool passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                   root_task_selector, root_resource_selector, parent_resource,
                                   8U) == success;
        passed = sys::certification::control(
                     sys::test_abi::v1::control_operation::memory_inject_extent_failure, 1U) ==
                     success &&
                 passed;
        passed =
            sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                         root_task_selector, parent_resource, child_resource, 1U) == no_memory &&
            passed;
        passed = sys::certification::control(
                     sys::test_abi::v1::control_operation::memory_inject_extent_failure, 0U) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              parent_resource) == success &&
                 passed;

        /* Repeatedly drive one resource to its quota and reclaim every page. */
        for (sys::word_t cycle = 0U; cycle < cycles && passed; ++cycle) {
            passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                  root_task_selector, root_resource_selector, parent_resource,
                                  frame_count) == success;
            for (sys::word_t index = 0U; index < frame_count && passed; ++index) {
                passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                                      parent_resource, first_frame + index) == success;
            }
            passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                                  parent_resource, overflow_frame) == no_memory &&
                     passed;
            for (sys::word_t index = 0U; index < frame_count; ++index) {
                passed = sys::control(sys::abi::v1::control_operation::frame_destroy,
                                      first_frame + index) == success &&
                         passed;
            }
            passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                                  parent_resource) == success &&
                     passed;
        }

        sys::word_t after = 0U;
        passed = sys::certification::control_result1(
                     after, sys::test_abi::v1::control_operation::memory_invariant_snapshot) ==
                     success &&
                 passed;
        return passed && before == after;
    }

    [[nodiscard]] bool test_memory_resource_lifecycle() noexcept {
        constexpr sys::word_t first_frame = 16U;
        constexpr sys::word_t frame_total = 8U;
        constexpr sys::word_t first_table = 24U;
        constexpr sys::word_t table_total = 4U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        sys::word_t owned_before = 0U;
        if (sys::control_result1(owned_before, sys::abi::v1::control_operation::memory_query) !=
            success)
            return false;

        for (sys::word_t index = 0U; index < frame_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::frame_create, 0U,
                             first_frame + index) != success)
                return false;
        }
        for (sys::word_t index = 0U; index < table_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::page_table_create, 0U,
                             first_table + index, 3U) != success)
                return false;
        }

        sys::word_t owned_peak = 0U;
        if (sys::control_result1(owned_peak, sys::abi::v1::control_operation::memory_query) !=
                success ||
            owned_peak != owned_before + frame_total + table_total)
            return false;

        for (sys::word_t index = 0U; index < table_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::page_table_destroy,
                             first_table + index) != success)
                return false;
        }
        for (sys::word_t index = 0U; index < frame_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::frame_destroy, first_frame + index) !=
                success)
                return false;
        }

        sys::word_t owned_after = 0U;
        return sys::control_result1(owned_after, sys::abi::v1::control_operation::memory_query) ==
                   success &&
               owned_after == owned_before;
    }

    [[nodiscard]] bool test_memory_mapping_database() noexcept {
        constexpr sys::word_t frame_selector = 16U;
        constexpr sys::word_t root_space_selector = 3U;
        constexpr sys::word_t first_address = 0x2000a000U;
        constexpr sys::word_t second_address = 0x2000b000U;
        constexpr sys::word_t read_write = 3U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t busy = static_cast<sys::word_t>(sys::error_t::busy);

        if (sys::control(sys::abi::v1::control_operation::frame_create, 0U, frame_selector) !=
            success)
            return false;

        bool passed = sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                                   frame_selector, first_address, read_write, 0x100U) == success;
        passed = sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                              frame_selector, second_address, read_write, 0x100U) == success &&
                 passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) == busy &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                              frame_selector, first_address) == success &&
                 passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) == busy &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                              frame_selector, second_address) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) ==
                     success &&
                 passed;

        if (!passed) {
            (void)sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                               frame_selector, first_address);
            (void)sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                               frame_selector, second_address);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector);
        }
        return passed;
    }

    [[nodiscard]] bool test_memory_authority_revoke() noexcept {
        constexpr sys::word_t frame_selector = 16U;
        constexpr sys::word_t derived_selector = 17U;
        constexpr sys::word_t root_space_selector = 3U;
        constexpr sys::word_t address = 0x20006000U;
        constexpr sys::word_t read_write = 3U;
        constexpr sys::word_t cap_write = 1U << 1U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t not_found = static_cast<sys::word_t>(sys::error_t::not_found);

        if (sys::control(sys::abi::v1::control_operation::frame_create, 0U, frame_selector) !=
            success)
            return false;
        bool passed = sys::control(sys::abi::v1::control_operation::capability_copy, 0U,
                                   derived_selector, frame_selector, cap_write) == success;
        passed = sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                              derived_selector, address, read_write, 0x100U) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::capability_revoke, 0U, 0U,
                              frame_selector) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::capability_delete, 0U,
                              derived_selector) == not_found &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) ==
                     success &&
                 passed;

        if (!passed) {
            (void)sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                               frame_selector, address);
            (void)sys::control(sys::abi::v1::control_operation::capability_delete, 0U,
                               derived_selector);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector);
        }
        return passed;
    }

    [[nodiscard]] bool test_memory_attributes_pressure() noexcept {
        constexpr sys::word_t device_selector = 16U;
        constexpr sys::word_t frame_selector = 17U;
        constexpr sys::word_t root_space_selector = 3U;
        constexpr sys::word_t device_address = 0x20007000U;
        constexpr sys::word_t normal_address = 0x20008000U;
        constexpr sys::word_t read_write = 3U;
        constexpr sys::word_t read_execute = 5U;
        constexpr sys::word_t device_outer = 0x201U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t denied = static_cast<sys::word_t>(sys::error_t::denied);

        bool passed = sys::control(sys::abi::v1::control_operation::device_frame_create,
                                   device_selector, 0x09000000U) == success;
        passed =
            sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                         device_selector, device_address, read_execute, device_outer) == denied &&
            passed;
        passed =
            sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                         device_selector, device_address, read_write, device_outer) == success &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                              device_selector, device_address) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, device_selector) ==
                     success &&
                 passed;

        sys::word_t owned_before = 0U;
        passed = sys::control_result1(owned_before,
                                      sys::abi::v1::control_operation::memory_query) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_create, 0U, frame_selector) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                              frame_selector, normal_address, read_write, device_outer) == denied &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) ==
                     success &&
                 passed;
        sys::word_t owned_after = 0U;
        passed = sys::control_result1(owned_after, sys::abi::v1::control_operation::memory_query) ==
                     success &&
                 owned_after == owned_before && passed;
        return passed;
    }

    struct acceptance_ledger {
        sys::word_t failure_mask{};
        sys::word_t failure_count{};
        bool transport_ok{true};
    };

    void record(acceptance_ledger& ledger, test_id id, bool test_pass) noexcept {
        const sys::word_t numeric_id = static_cast<sys::word_t>(id);
        const bool reported =
            sys::certification::control(sys::test_abi::v1::control_operation::acceptance_report,
                                        numeric_id, test_pass ? 1U : 0U) ==
            static_cast<sys::word_t>(sys::error_t::success);

        if (!test_pass) {
            ledger.failure_mask |= sys::word_t{1U} << (numeric_id - 1U);
            ++ledger.failure_count;
        }
        ledger.transport_ok = ledger.transport_ok && (reported || !test_pass);
    }

    [[nodiscard]] sys::word_t xorshift(sys::word_t& state) noexcept {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        return state;
    }

    [[noreturn]] void worker(sys::word_t role, sys::word_t seed) noexcept {
        sys::word_t state = seed ^ (role * 0x9e3779b97f4a7c15ULL);
        for (;;) {
            const sys::word_t selector = xorshift(state) & 3U;
            sys::word_t result = 0U;
            sys::word_t expected = 0U;
            switch (selector) {
                case 0U:
                    result = sys::control(sys::abi::v1::control_operation::notification_poll, 31U);
                    expected = static_cast<sys::word_t>(sys::error_t::denied);
                    break;
                case 1U:
                    result = sys::control(sys::abi::v1::control_operation::thread_suspend, 31U);
                    expected = static_cast<sys::word_t>(sys::error_t::denied);
                    break;
                case 2U:
                    result = sys::control(sys::abi::v1::control_operation::map_frame, 31U, 31U,
                                          0x20002000U, 1U);
                    expected = static_cast<sys::word_t>(sys::error_t::denied);
                    break;
                default:
                    result =
                        sys::control(sys::abi::v1::control_operation::capability_delete, 0U, 31U);
                    expected = static_cast<sys::word_t>(sys::error_t::not_found);
                    break;
            }
            const sys::word_t failure = result == expected ? 0U : 1U;
            (void)sys::certification::control(
                sys::test_abi::v1::control_operation::acceptance_worker_tick, failure, role, state);
        }
    }

    [[nodiscard]] bool create_worker(sys::word_t cpu) noexcept {
        return sys::control(sys::abi::v1::control_operation::child_create, cpu, cpu,
                            worker_thread_selector_base + cpu, worker_task_selector_base + cpu,
                            worker_space_selector_base + cpu) ==
               static_cast<sys::word_t>(sys::error_t::success);
    }

    [[nodiscard]] bool wait_workers() noexcept {
        for (sys::word_t spins = 0U; spins < 10000000U; ++spins) {
            bool complete = true;
            for (sys::word_t cpu = 1U; cpu < 4U; ++cpu) {
                const sys::word_t operations = sys::certification::control(
                    sys::test_abi::v1::control_operation::acceptance_query, cpu, 0U);
                const sys::word_t failures = sys::certification::control(
                    sys::test_abi::v1::control_operation::acceptance_query, cpu, 1U);
                if (operations < worker_threshold || failures != 0U) {
                    complete = false;
                }
            }
            if (complete)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool stop_destroy_worker(sys::word_t cpu) noexcept {
        const sys::word_t thread_selector = worker_thread_selector_base + cpu;
        const sys::word_t task_selector = worker_task_selector_base + cpu;
        const sys::word_t space_selector = worker_space_selector_base + cpu;
        const sys::word_t suspend =
            sys::control(sys::abi::v1::control_operation::thread_suspend, thread_selector);
        if (suspend != static_cast<sys::word_t>(sys::error_t::success)) {
            return false;
        }
        for (sys::word_t attempt = 0U; attempt < 100000U; ++attempt) {
            const sys::word_t result = sys::control(sys::abi::v1::control_operation::child_destroy,
                                                    thread_selector, task_selector, space_selector);
            if (result == static_cast<sys::word_t>(sys::error_t::success)) {
                return true;
            }
            if (result != static_cast<sys::word_t>(sys::error_t::busy)) {
                return false;
            }
        }
        return false;
    }
} // namespace

extern "C" int main(sys::word_t argument0, sys::word_t argument1) noexcept {
    if (argument0 != 0U)
        worker(argument0, argument1);

    acceptance_ledger ledger{};
    record(ledger, test_id::root_only_boot, true);
    record(ledger, test_id::bootinfo_contract, true);

    const sys::word_t poll = sys::control(sys::abi::v1::control_operation::notification_poll, 14U);
    record(ledger, test_id::notification_control,
           poll == static_cast<sys::word_t>(sys::error_t::success));

    constexpr sys::word_t cap_read = 1U;
    constexpr sys::word_t cap_grant = 1U << 3U;
    bool capability_pass = true;
    for (sys::word_t iteration = 0U; iteration < 128U; ++iteration) {
        const sys::word_t mint = sys::control(sys::abi::v1::control_operation::capability_mint, 0U,
                                              16U, 14U, cap_read | cap_grant, iteration + 1U);
        const sys::word_t copy =
            sys::control(sys::abi::v1::control_operation::capability_copy, 0U, 17U, 16U, cap_read);
        const sys::word_t escalate = sys::control(sys::abi::v1::control_operation::capability_mint,
                                                  0U, 18U, 16U, cap_read | (1U << 1U));
        const sys::word_t revoke =
            sys::control(sys::abi::v1::control_operation::capability_revoke, 0U, 0U, 14U);
        const sys::word_t deleted_child =
            sys::control(sys::abi::v1::control_operation::capability_delete, 0U, 16U);
        const sys::word_t deleted_grandchild =
            sys::control(sys::abi::v1::control_operation::capability_delete, 0U, 17U);
        capability_pass = capability_pass &&
                          mint == static_cast<sys::word_t>(sys::error_t::success) &&
                          copy == static_cast<sys::word_t>(sys::error_t::success) &&
                          escalate == static_cast<sys::word_t>(sys::error_t::denied) &&
                          revoke == static_cast<sys::word_t>(sys::error_t::success) &&
                          deleted_child == static_cast<sys::word_t>(sys::error_t::not_found) &&
                          deleted_grandchild == static_cast<sys::word_t>(sys::error_t::not_found);
        if (!capability_pass)
            break;
    }
    record(ledger, test_id::capability_control, capability_pass);

    const sys::word_t hv_self_test =
        sys::certification::control(sys::test_abi::v1::control_operation::hypervisor_self_test);
    const bool hypervisor_pass = hv_self_test == static_cast<sys::word_t>(sys::error_t::success);
    record(ledger, test_id::hypervisor_real_single_vcpu, hypervisor_pass);
    record(ledger, test_id::hypervisor_control_model_0_4, hypervisor_pass);
    sys::word_t hv_fuzz_failures = 0U;
    for (sys::word_t iteration = 0U; iteration < 4096U; ++iteration) {
        const sys::word_t result = sys::hypervisor_invoke(
            sys::abi::v1::hypervisor_operation::stage2_map, 31U, iteration << 12U, 0x48000000U, 1U);
        if (result != static_cast<sys::word_t>(sys::error_t::denied))
            ++hv_fuzz_failures;
    }
    record(ledger, test_id::hypervisor_negative_fuzz, hv_fuzz_failures == 0U);

    const bool ipc_lifecycle_pass = test_ipc_lifecycle_races();
    record(ledger, test_id::ipc_lifecycle_races, ipc_lifecycle_pass);
    const bool capability_race_pass = test_capability_transfer_revoke_race();
    record(ledger, test_id::capability_transfer_revoke_race, capability_race_pass);
    record(ledger, test_id::ipc_capability_batch, capability_race_pass);
    const bool object_race_pass = test_object_lookup_destroy_race();
    record(ledger, test_id::object_lookup_destroy_race, object_race_pass);
    const bool scheduling_configuration_pass = test_scheduling_configuration();
    record(ledger, test_id::scheduling_configuration, scheduling_configuration_pass);

    const service_results services = run_userspace_services();
    record(ledger, test_id::userspace_pager_service, services.pager);
    record(ledger, test_id::memory_server_protocol, services.memory_protocol);
    record(ledger, test_id::ipc_ool_frame_grant, services.memory_protocol);

    const bool dynamic_ipc_pass = test_dynamic_ipc_objects();
    record(ledger, test_id::dynamic_ipc_objects, dynamic_ipc_pass);
    record(ledger, test_id::ipc_completion_gate,
           ipc_lifecycle_pass && capability_race_pass && object_race_pass &&
               services.memory_protocol && dynamic_ipc_pass);

    const bool memory_lifecycle_pass = test_memory_resource_lifecycle();
    record(ledger, test_id::memory_resource_lifecycle, memory_lifecycle_pass);

    const bool mapping_database_pass = test_memory_mapping_database();
    record(ledger, test_id::memory_mapping_database, mapping_database_pass);
    const bool authority_revoke_pass = test_memory_authority_revoke();
    record(ledger, test_id::memory_authority_revoke, authority_revoke_pass);
    const bool attributes_pressure_pass = test_memory_attributes_pressure();
    record(ledger, test_id::memory_attributes_pressure, attributes_pressure_pass);
    const bool resource_delegation_pass = test_memory_resource_delegation();
    record(ledger, test_id::memory_resource_delegation, resource_delegation_pass);
    const bool extent_retype_pass = test_memory_extent_retype();
    record(ledger, test_id::memory_extent_retype, extent_retype_pass);
    const bool extent_metadata_pass = test_memory_extent_metadata();
    record(ledger, test_id::memory_extent_metadata, extent_metadata_pass);
    const bool pressure_rollback_pass = test_memory_pressure_rollback();
    record(ledger, test_id::memory_pressure_rollback, pressure_rollback_pass);

    bool created = true;
    for (sys::word_t cpu = 1U; cpu < 4U; ++cpu) {
        created = create_worker(cpu) && created;
    }
    record(ledger, test_id::root_created_objects, created);

    const bool fuzz_pass = created && wait_workers();
    record(ledger, test_id::root_created_smp_fuzz, fuzz_pass);

    bool destroyed = true;
    for (sys::word_t cpu = 1U; cpu < 4U; ++cpu) {
        destroyed = stop_destroy_worker(cpu) && destroyed;
    }

    bool reused = false;
    if (destroyed) {
        reused = create_worker(1U);
        if (reused)
            reused = stop_destroy_worker(1U);
    }
    record(ledger, test_id::object_destroy_reuse, destroyed && reused);
    record(ledger, test_id::memory_completion_gate,
           services.pager && services.memory_protocol && memory_lifecycle_pass &&
               mapping_database_pass && authority_revoke_pass && attributes_pressure_pass &&
               resource_delegation_pass && extent_retype_pass && extent_metadata_pass &&
               pressure_rollback_pass && fuzz_pass && destroyed && reused);
    record(ledger, test_id::capability_completion_gate,
           capability_pass && capability_race_pass && object_race_pass && authority_revoke_pass &&
               services.memory_protocol && dynamic_ipc_pass && fuzz_pass && destroyed && reused);
    record(ledger, test_id::scheduler_completion_gate,
           scheduling_configuration_pass && ipc_lifecycle_pass && capability_race_pass &&
               services.pager && services.memory_protocol && fuzz_pass && destroyed && reused);

    const bool pass = ledger.failure_count == 0U && ledger.transport_ok;
    (void)sys::certification::control(sys::test_abi::v1::control_operation::acceptance_finalize,
                                      pass ? 1U : 0U, ledger.failure_count, ledger.failure_mask,
                                      ledger.transport_ok ? 1U : 0U);
    return pass ? 0 : 1;
}

#else
#include <sys/types.hh>
extern "C" int main(sys::word_t, sys::word_t) noexcept {
    return 0;
}
#endif
