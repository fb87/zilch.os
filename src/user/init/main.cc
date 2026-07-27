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
                                               sys::word_t space_selector) noexcept {
        const sys::word_t suspend =
            sys::control(sys::abi::v1::control_operation::thread_suspend, thread_selector);
        if (suspend != static_cast<sys::word_t>(sys::error_t::success)) {
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
                return false;
            }
        }
        return false;
    }

    [[nodiscard]] bool wait_for_badge(sys::word_t expected) noexcept {
        for (sys::word_t spin = 0U; spin < 10000000U; ++spin) {
            sys::word_t badges = 0U;
            const sys::word_t status = sys::control_result1(
                badges, sys::abi::v1::control_operation::notification_poll, 14U);
            if (status == static_cast<sys::word_t>(sys::error_t::success)) {
                if ((badges & 0xffff0000U) != 0U)
                    return false;
                if ((badges & expected) != 0U)
                    return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool run_userspace_pager_service() noexcept {
        constexpr sys::word_t server_thread = 17U;
        constexpr sys::word_t server_task = 20U;
        constexpr sys::word_t server_space = 23U;
        constexpr sys::word_t client_thread = 18U;
        constexpr sys::word_t client_task = 21U;
        constexpr sys::word_t client_space = 24U;

        if (!create_service_process(1U, memory_server_role, server_thread, server_task,
                                    server_space))
            return false;

        bool pass =
            create_service_process(2U, fault_client_role, client_thread, client_task, client_space);
        if (pass)
            pass = wait_for_badge(1U);
        if (pass)
            pass = destroy_service_process(client_thread, client_task, client_space);

        if (pass)
            pass = create_service_process(3U, second_fault_client_role, client_thread, client_task,
                                          client_space);
        if (pass)
            pass = wait_for_badge(2U);
        if (pass)
            pass = destroy_service_process(client_thread, client_task, client_space);

        const bool server_destroyed =
            destroy_service_process(server_thread, server_task, server_space);
        return pass && server_destroyed;
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

    [[nodiscard]] bool report(test_id id, bool pass) noexcept {
        return sys::certification::control(sys::test_abi::v1::control_operation::acceptance_report,
                                           static_cast<sys::word_t>(id), pass ? 1U : 0U) ==
               static_cast<sys::word_t>(sys::error_t::success);
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

    bool pass = true;
    pass = report(test_id::root_only_boot, true) && pass;
    pass = report(test_id::bootinfo_contract, true) && pass;

    const sys::word_t poll = sys::control(sys::abi::v1::control_operation::notification_poll, 14U);
    pass = report(test_id::notification_control,
                  poll == static_cast<sys::word_t>(sys::error_t::success)) &&
           pass;

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
    pass = report(test_id::capability_control, capability_pass) && pass;

    const sys::word_t hv_self_test =
        sys::certification::control(sys::test_abi::v1::control_operation::hypervisor_self_test);
    const bool hypervisor_pass = hv_self_test == static_cast<sys::word_t>(sys::error_t::success);
    pass = report(test_id::hypervisor_real_single_vcpu, hypervisor_pass) && pass;
    pass = report(test_id::hypervisor_control_model_0_4, hypervisor_pass) && pass;
    sys::word_t hv_fuzz_failures = 0U;
    for (sys::word_t iteration = 0U; iteration < 4096U; ++iteration) {
        const sys::word_t result = sys::hypervisor_invoke(
            sys::abi::v1::hypervisor_operation::stage2_map, 31U, iteration << 12U, 0x48000000U, 1U);
        if (result != static_cast<sys::word_t>(sys::error_t::denied))
            ++hv_fuzz_failures;
    }
    pass = report(test_id::hypervisor_negative_fuzz, hv_fuzz_failures == 0U) && pass;

    const bool pager_service_pass = run_userspace_pager_service();
    pass = report(test_id::userspace_pager_service, pager_service_pass) && pass;

    const bool dynamic_ipc_pass = test_dynamic_ipc_objects();
    pass = report(test_id::dynamic_ipc_objects, dynamic_ipc_pass) && pass;

    bool created = true;
    for (sys::word_t cpu = 1U; cpu < 4U; ++cpu) {
        created = create_worker(cpu) && created;
    }
    pass = report(test_id::root_created_objects, created) && pass;

    const bool fuzz_pass = created && wait_workers();
    pass = report(test_id::root_created_smp_fuzz, fuzz_pass) && pass;

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
    pass = report(test_id::object_destroy_reuse, destroyed && reused) && pass;

    (void)sys::certification::control(sys::test_abi::v1::control_operation::acceptance_finalize,
                                      pass ? 1U : 0U);
    return pass ? 0 : 1;
}

#else
#include <sys/types.hh>
extern "C" int main(sys::word_t, sys::word_t) noexcept {
    return 0;
}
#endif
