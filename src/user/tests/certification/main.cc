#include <sys/certification.hh>
#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/control_plane_certification.hh>
#include <sys/domain_manager.hh>
#include <sys/hypervisor.hh>
#include <sys/hypervisor_lifecycle_certification.hh>
#include <sys/ipc.hh>
#include <sys/memory_certification.hh>
#include <sys/test_abi/v1/certification.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
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
        interrupt_timer_platform_gate = 32U,
        security_hardening_gate = 33U,
        kernel_core_1_0_gate = 34U,
        userspace_control_plane_graph = 35U,
        domain_manager_api = 36U,
        hypervisor_dynamic_lifecycle = 37U,
        hypervisor_vm_create = 38U,
        hypervisor_vcpu_create = 39U,
        hypervisor_vm_busy = 40U,
        hypervisor_vcpu_destroy = 41U,
        hypervisor_vcpu_stale = 42U,
        hypervisor_vm_destroy = 43U,
        hypervisor_vm_stale = 44U,
        hypervisor_vm_reuse = 45U,
        domain_guest_load = 46U,
        domain_guest_run = 47U,
        thread_create_lifecycle = 48U,
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

    /*
     * thread_create's own role namespace: 0x301, clear of the 1-4 CPU-seed
     * "roles" worker() uses, the 0x100-range service roles above, and
     * root_graph.hh's control_plane_role (0x200-0x204) / dynamic_launch_role
     * (0x300) -- this legacy harness never includes root_graph.hh, so this
     * is a local, independent choice, not a shared constant.
     */
    inline constexpr sys::word_t thread_create_test_role = 0x301U;
    inline constexpr sys::word_t thread_create_test_notification_selector = 200U;
    inline constexpr sys::word_t thread_create_test_thread_selector = 201U;
    inline constexpr sys::word_t thread_create_test_space_selector = 202U;
    inline constexpr sys::word_t thread_create_test_endpoint_selector = 203U;
    inline constexpr sys::word_t thread_create_test_badge = 1U;

    /*
     * Proves create_user_thread() (thread_create) actually shares the
     * caller's cspace rather than merely succeeding: this entry point only
     * has a working capability at thread_create_test_notification_selector
     * because it inherited root's cspace wholesale -- nothing was minted
     * into it individually, there is nothing to mint since sharing a
     * cspace is the entire mechanism. Signals once to prove it is alive
     * and can act through that shared capability, then parks on a
     * dedicated endpoint (created before this thread is spawned, so no
     * ordering race) via a genuinely blocking ipc_receive -- NOT a
     * notification_poll busy-loop, which pinned this thread's CPU at 100%
     * forever and starved whatever else needed it, hanging the rest of the
     * test run. There is no thread_destroy to clean this thread up with,
     * so it persists harmlessly (one thread slot, and now zero CPU) for
     * the rest of this run, same as the existing worker() test threads
     * already do.
     */
    [[noreturn]] void thread_create_test_entry(sys::word_t, sys::word_t) noexcept {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal,
                           thread_create_test_notification_selector, thread_create_test_badge);
        for (;;)
            (void)sys::ipc_receive(thread_create_test_endpoint_selector);
    }

    [[nodiscard]] bool test_thread_create_lifecycle() noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        if (sys::control(sys::abi::v1::control_operation::notification_create,
                         thread_create_test_notification_selector) != success)
            return false;
        // Created before thread_create, not after: unlike minting a
        // capability into a freshly process_create()d child's own new
        // cspace (which must happen after -- the destination task
        // capability doesn't exist yet), this endpoint lands in the SAME
        // cspace thread_create's new thread will share, so there is no
        // ordering race to worry about here.
        if (sys::control(sys::abi::v1::control_operation::endpoint_create,
                         thread_create_test_endpoint_selector) != success)
            return false;
        // Negative case: identical thread/space selectors must be rejected
        // before anything is allocated.
        if (sys::control(sys::abi::v1::control_operation::thread_create, 1U,
                         thread_create_test_role, thread_create_test_thread_selector,
                         thread_create_test_thread_selector) !=
            static_cast<sys::word_t>(sys::error_t::invalid_argument))
            return false;
        if (sys::control(sys::abi::v1::control_operation::thread_create, 1U,
                         thread_create_test_role, thread_create_test_thread_selector,
                         thread_create_test_space_selector) != success)
            return false;
        for (sys::word_t spins = 0U; spins < 10000000U; ++spins) {
            sys::word_t badges = 0U;
            const sys::word_t status =
                sys::control_result1(badges, sys::abi::v1::control_operation::notification_poll,
                                     thread_create_test_notification_selector);
            if (status == success && (badges & thread_create_test_badge) != 0U)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool create_service_process(sys::word_t cpu, sys::word_t role,
                                              sys::word_t thread_selector,
                                              sys::word_t task_selector,
                                              sys::word_t space_selector) noexcept {
        return sys::control(sys::abi::v1::control_operation::process_create, cpu, role,
                            thread_selector, task_selector,
                            space_selector) == static_cast<sys::word_t>(sys::error_t::success);
    }

    /*
     * root_graph.hh's production path mints the UART device frame into
     * the console-server's cspace right after launching it
     * (mint_console_uart_frame()) so it can drive real hardware. This
     * legacy harness's generic create_service_process() has no
     * equivalent step for any role, so the console-server process it
     * creates would call map_uart() against a capability slot that was
     * never populated -- map_uart()'s bounded retry would spin through
     * its whole attempt budget failing every time, then report failure
     * instead of ready, hanging the combined-ready wait() forever.
     * Mirror the same device-frame setup here, but only for the console
     * role -- every other role is unaffected.
     */
    [[nodiscard]] bool create_console_service_process(sys::word_t cpu, sys::word_t role,
                                                      sys::word_t thread_selector,
                                                      sys::word_t task_selector,
                                                      sys::word_t space_selector) noexcept {
        if (!create_service_process(cpu, role, thread_selector, task_selector, space_selector))
            return false;
        if (role != static_cast<sys::word_t>(sys::abi::v1::control_plane_role::console))
            return true;
        /*
         * console-server no longer touches raw hardware (it forwards to
         * serial-driver, which this isolated harness never launches -- see
         * root_graph.hh for the production wiring), but it unconditionally
         * spawns a second thread at startup (thread_create, not process_
         * create -- see console/main.cc's stdin_main()) that blocks on its
         * own dedicated endpoint. Without a real endpoint minted at the
         * slot it expects (console-server's local slot 12), that blocking
         * receive would fail its capability lookup immediately instead of
         * genuinely blocking, and loop forever doing so -- confirmed
         * hanging the whole harness the first time this was tested. This
         * mints a real (if never-signaled) endpoint so it blocks properly
         * instead; nothing in this harness exercises byte-level read_byte/
         * write/write_byte, so nothing else needs wiring here.
         */
        constexpr sys::capability_id_t console_stdin_root_endpoint_selector = 70U;
        constexpr sys::capability_id_t console_stdin_child_endpoint_selector = 12U;
        constexpr sys::word_t read_write =
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        if (sys::control(sys::abi::v1::control_operation::endpoint_create,
                         console_stdin_root_endpoint_selector) != success)
            return false;
        return sys::control(sys::abi::v1::control_operation::capability_mint, task_selector,
                            console_stdin_child_endpoint_selector,
                            console_stdin_root_endpoint_selector, read_write) == success;
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

    [[nodiscard]] bool test_domain_manager_api() noexcept {
        constexpr sys::capability_id_t vm_selector = 61U;
        constexpr sys::capability_id_t vcpu_selector = 62U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        sys::domain_manager::manager domain{};
        bool passed = domain.create(vm_selector, vcpu_selector, 0U, 0x3000U) == success;
        if (passed)
            passed = domain.configure(0x40000000U, 0x3c5U, 0x50000000U) == success;
        if (passed)
            passed = domain.destroy() == success;
        return passed;
    }

    [[nodiscard]] bool test_domain_manager_lifecycle(sys::capability_id_t endpoint) noexcept {
        constexpr sys::capability_id_t domain_task_selector = 50U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        /* The guest's UART is vPL011-emulated (trap-and-emulate through
         * the console-server), not passed through directly -- see
         * root_graph.hh::start_embedded_guest(), which likewise no longer
         * hands the domain-manager a direct device_frame_create/
         * interrupt_create capability for it. The guest manifest lists
         * zero devices now, so there is nothing for a manifest-driven
         * passthrough loop to set up either; the only setup the
         * domain-manager still needs is the console-server endpoint
         * below, for vPL011 to forward real character I/O through. */
        /* control_plane_certification::run() creates the console role's
         * endpoint at endpoint_base(16) + console_index(2) = 18 in this
         * process's own cspace, and that role has already been created
         * and health-checked by the time probe() reaches index 3 (domain)
         * -- see control_plane_certification.hh's health-check loop, which
         * runs before the probe loop. Mint it into the domain-manager's
         * cspace at slot 19, matching domain/main.cc's
         * console_endpoint_selector, mirroring
         * root_graph.hh::start_embedded_guest() on the production path
         * now that the guest's UART is vPL011-emulated rather than
         * passed through directly. */
        constexpr sys::capability_id_t local_console_endpoint_selector = 18U;
        constexpr sys::capability_id_t domain_console_endpoint_selector = 19U;
        constexpr sys::word_t write_only =
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        if (sys::control(sys::abi::v1::control_operation::capability_mint, domain_task_selector,
                         domain_console_endpoint_selector, local_console_endpoint_selector,
                         write_only) != success)
            return false;
        const auto launch = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::launch), 0U);
        if (launch.status != success || launch.message0 != success ||
            launch.message1 != static_cast<sys::word_t>(sys::domain_manager::state::created))
            return false;

        const auto load = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::load), 0U);
        const bool load_pass =
            load.status == success && load.message0 == success &&
            load.message1 == static_cast<sys::word_t>(sys::domain_manager::state::runnable);
        (void)sys::certification::control(sys::test_abi::v1::control_operation::acceptance_report,
                                          static_cast<sys::word_t>(test_id::domain_guest_load),
                                          load_pass ? 1U : 0U);
        if (!load_pass) {
            (void)sys::certification::control(
                sys::test_abi::v1::control_operation::domain_manager_detail, load.message1);
            return false;
        }

#if CONFIG_DOMAIN_GUEST_INTERACTIVE
        const auto served = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::serve), 0U);
        (void)sys::certification::control(
            sys::test_abi::v1::control_operation::domain_manager_detail,
            served.message1 != 0U ? served.message1 : served.message0 & 0xffffffffU);
        return false;
#else
        const auto run = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::run), 0U);
        const bool run_pass = run.status == success && run.message0 == success;
        (void)sys::certification::control(sys::test_abi::v1::control_operation::acceptance_report,
                                          static_cast<sys::word_t>(test_id::domain_guest_run),
                                          run_pass ? 1U : 0U);
        if (!run_pass)
            return false;
#endif

        const auto destroy = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::destroy), 0U);
        if (destroy.status != success || destroy.message0 != success ||
            destroy.message1 != static_cast<sys::word_t>(sys::domain_manager::state::empty))
            return false;

        const auto relaunch = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::launch), 0U);
        if (relaunch.status != success || relaunch.message0 != success ||
            relaunch.message1 != static_cast<sys::word_t>(sys::domain_manager::state::created))
            return false;

        const bool destroyed =
            sys::ipc_call(endpoint,
                          static_cast<sys::word_t>(sys::abi::v1::control_plane_operation::destroy),
                          0U)
                .status == success;
        return destroyed;
    }

    [[nodiscard]] bool test_memory_authority_revoke() noexcept {
        constexpr sys::word_t frame_selector = 16U;
        constexpr sys::word_t derived_selector = 17U;
        constexpr sys::word_t root_space_selector = 3U;
        /*
         * Keep the authority probe above the certification image's loadable
         * segments and below the fixed bootstrap stack page.
         */
        constexpr sys::word_t address = 0x2000e000U;
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
    if (argument0 == thread_create_test_role)
        thread_create_test_entry(argument0, argument1);
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
    record(ledger, test_id::thread_create_lifecycle, test_thread_create_lifecycle());

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

    const bool memory_lifecycle_pass = sys::memory_certification::resource_lifecycle();
    record(ledger, test_id::memory_resource_lifecycle, memory_lifecycle_pass);

    const bool mapping_database_pass = sys::memory_certification::mapping_database();
    record(ledger, test_id::memory_mapping_database, mapping_database_pass);
    const bool authority_revoke_pass = test_memory_authority_revoke();
    record(ledger, test_id::memory_authority_revoke, authority_revoke_pass);
    const bool attributes_pressure_pass = test_memory_attributes_pressure();
    record(ledger, test_id::memory_attributes_pressure, attributes_pressure_pass);
    const bool resource_delegation_pass = sys::memory_certification::resource_delegation();
    record(ledger, test_id::memory_resource_delegation, resource_delegation_pass);
    const bool extent_retype_pass = sys::memory_certification::extent_retype();
    record(ledger, test_id::memory_extent_retype, extent_retype_pass);
    const bool extent_metadata_pass = sys::memory_certification::extent_metadata();
    record(ledger, test_id::memory_extent_metadata, extent_metadata_pass);
    const bool pressure_rollback_pass = sys::memory_certification::pressure_rollback();
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
    record(ledger, test_id::interrupt_timer_platform_gate,
           scheduling_configuration_pass && ipc_lifecycle_pass && capability_race_pass &&
               dynamic_ipc_pass && fuzz_pass && destroyed && reused);
    record(ledger, test_id::security_hardening_gate,
           capability_pass && hypervisor_pass && ipc_lifecycle_pass && capability_race_pass &&
               object_race_pass && services.pager && services.memory_protocol &&
               memory_lifecycle_pass && mapping_database_pass && authority_revoke_pass &&
               pressure_rollback_pass && fuzz_pass && destroyed && reused);
    record(ledger, test_id::kernel_core_1_0_gate,
           capability_pass && ipc_lifecycle_pass && capability_race_pass && object_race_pass &&
               scheduling_configuration_pass && services.pager && services.memory_protocol &&
               dynamic_ipc_pass && memory_lifecycle_pass && mapping_database_pass &&
               authority_revoke_pass && pressure_rollback_pass && fuzz_pass && destroyed && reused);
    record(ledger, test_id::userspace_control_plane_graph,
           sys::control_plane_certification::run(
               create_console_service_process, destroy_service_process, wait_for_badges,
               [](sys::word_t index, sys::capability_id_t endpoint) noexcept {
                   return index != 3U ? true : test_domain_manager_lifecycle(endpoint);
               }));
    record(ledger, test_id::domain_manager_api, test_domain_manager_api());
    const sys::word_t hypervisor_lifecycle = sys::hypervisor_lifecycle_certification::run();
    record(ledger, test_id::hypervisor_dynamic_lifecycle,
           hypervisor_lifecycle == sys::hypervisor_lifecycle_certification::complete_mask);
    for (sys::word_t stage = 0U; stage < 8U; ++stage)
        record(
            ledger,
            static_cast<test_id>(static_cast<sys::word_t>(test_id::hypervisor_vm_create) + stage),
            (hypervisor_lifecycle & (1U << stage)) != 0U);

    const bool pass = ledger.failure_count == 0U && ledger.transport_ok;
    (void)sys::certification::control(sys::test_abi::v1::control_operation::acceptance_finalize,
                                      pass ? 1U : 0U, ledger.failure_count, ledger.failure_mask,
                                      ledger.transport_ok ? 1U : 0U);
    return pass ? 0 : 1;
}
