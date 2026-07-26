#include <abi/sys/v1/control.hh>
#include <sys/control.hh>
#include <sys/types.hh>

namespace
{
    enum class test_id : sys::word_t
    {
        root_only_boot = 1U,
        bootinfo_contract = 2U,
        capability_control = 3U,
        notification_control = 4U,
        root_created_objects = 5U,
        root_created_smp_fuzz = 6U,
        object_destroy_reuse = 7U,
    };

    inline constexpr sys::word_t worker_threshold = 4096U;

    [[nodiscard]] bool report(test_id id, bool pass) noexcept
    {
        return sys::control(sys::abi::v1::control_operation::acceptance_report,
                            static_cast<sys::word_t>(id), pass ? 1U : 0U)
            == static_cast<sys::word_t>(sys::error_t::success);
    }

    [[nodiscard]] sys::word_t xorshift(sys::word_t& state) noexcept
    {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        return state;
    }

    [[noreturn]] void worker(sys::word_t role, sys::word_t seed) noexcept
    {
        sys::word_t state = seed ^ (role * 0x9e3779b97f4a7c15ULL);
        for (;;) {
            const sys::word_t selector = xorshift(state) & 3U;
            sys::word_t result = 0U;
            sys::word_t expected = 0U;
            switch (selector) {
            case 0U:
                result = sys::control(
                    sys::abi::v1::control_operation::notification_poll, 31U);
                expected = static_cast<sys::word_t>(sys::error_t::denied);
                break;
            case 1U:
                result = sys::control(
                    sys::abi::v1::control_operation::thread_suspend, 31U);
                expected = static_cast<sys::word_t>(sys::error_t::denied);
                break;
            case 2U:
                result = sys::control(
                    sys::abi::v1::control_operation::map_frame,
                    31U, 31U, 0x20002000U, 1U);
                expected = static_cast<sys::word_t>(sys::error_t::denied);
                break;
            default:
                result = sys::control(
                    sys::abi::v1::control_operation::capability_delete,
                    0U, 31U);
                expected = static_cast<sys::word_t>(sys::error_t::not_found);
                break;
            }
            const sys::word_t failure = result == expected ? 0U : 1U;
            (void)sys::control(
                sys::abi::v1::control_operation::acceptance_worker_tick,
                failure, role, state);
        }
    }

    [[nodiscard]] bool create_worker(sys::word_t cpu) noexcept
    {
        return sys::control(
                   sys::abi::v1::control_operation::child_create,
                   cpu, cpu, 19U + cpu, 22U + cpu, 25U + cpu)
            == static_cast<sys::word_t>(sys::error_t::success);
    }

    [[nodiscard]] bool wait_workers() noexcept
    {
        for (sys::word_t spins = 0U; spins < 10000000U; ++spins) {
            bool complete = true;
            for (sys::word_t cpu = 1U; cpu < 4U; ++cpu) {
                const sys::word_t operations = sys::control(
                    sys::abi::v1::control_operation::acceptance_query,
                    cpu, 0U);
                const sys::word_t failures = sys::control(
                    sys::abi::v1::control_operation::acceptance_query,
                    cpu, 1U);
                if (operations < worker_threshold || failures != 0U) {
                    complete = false;
                }
            }
            if (complete) return true;
        }
        return false;
    }

    [[nodiscard]] bool stop_destroy_worker(sys::word_t cpu) noexcept
    {
        const sys::word_t thread_selector = 19U + cpu;
        const sys::word_t task_selector = 22U + cpu;
        const sys::word_t space_selector = 25U + cpu;
        const sys::word_t suspend = sys::control(
            sys::abi::v1::control_operation::thread_suspend,
            thread_selector);
        if (suspend != static_cast<sys::word_t>(sys::error_t::success)) {
            return false;
        }
        for (sys::word_t attempt = 0U; attempt < 100000U; ++attempt) {
            const sys::word_t result = sys::control(
                sys::abi::v1::control_operation::child_destroy,
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
}

extern "C" int main(sys::word_t argument0, sys::word_t argument1) noexcept
{
    if (argument0 != 0U) worker(argument0, argument1);

    bool pass = true;
    pass = report(test_id::root_only_boot, true) && pass;
    pass = report(test_id::bootinfo_contract, true) && pass;

    const sys::word_t poll = sys::control(
        sys::abi::v1::control_operation::notification_poll, 14U);
    pass = report(test_id::notification_control,
                  poll == static_cast<sys::word_t>(sys::error_t::success))
        && pass;

    const sys::word_t copy = sys::control(
        sys::abi::v1::control_operation::capability_copy,
        0U, 16U, 14U, 1U);
    const sys::word_t remove = sys::control(
        sys::abi::v1::control_operation::capability_delete,
        0U, 16U);
    pass = report(test_id::capability_control,
                  copy == static_cast<sys::word_t>(sys::error_t::success)
                  && remove == static_cast<sys::word_t>(sys::error_t::success))
        && pass;

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
        if (reused) reused = stop_destroy_worker(1U);
    }
    pass = report(test_id::object_destroy_reuse, destroyed && reused) && pass;

    (void)sys::control(
        sys::abi::v1::control_operation::acceptance_finalize,
        pass ? 1U : 0U);
    return pass ? 0 : 1;
}
