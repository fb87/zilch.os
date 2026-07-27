#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/syscall.hh>
#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/ipc.hh>

namespace
{
    volatile sys::word_t bss_zero_probe;
    inline constexpr sys::word_t endpoint = 10U;
    inline constexpr sys::word_t fault_address = 0x20004000U;
    inline constexpr sys::word_t completion_magic = 0x50414745U;
    inline constexpr sys::word_t notification = 14U;
    inline constexpr sys::word_t lifecycle_role_base = 0x110U;
    inline constexpr sys::word_t capability_race_server_role = 0x115U;
    inline constexpr sys::word_t capability_race_sender_role = 0x116U;
    inline constexpr sys::word_t object_race_worker_role = 0x117U;
    inline constexpr sys::word_t undefined_instruction_role = 0x106U;
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    if (bss_zero_probe != 0U)
        return 4;
    bss_zero_probe = role;
    if (role == lifecycle_role_base) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           1U << 6U);
        const auto result = sys::ipc_call(endpoint, 0x43414e43U, 0U);
        if (result.status != static_cast<sys::word_t>(sys::error_t::timed_out))
            return 5;
        sys::thread_exit(0U, notification, 1U << 7U);
    }
    if (role == lifecycle_role_base + 1U) {
        sys::abi::v1::ipc_timeout timeout{};
        timeout.ticks = 2U;
        timeout.enabled = true;
        const auto result = sys::ipc_call(endpoint, 0x54494d45U, 0U, 0U, 0U, {}, timeout);
        if (result.status != static_cast<sys::word_t>(sys::error_t::timed_out))
            return 6;
        sys::thread_exit(0U, notification, 1U << 8U);
    }
    if (role == lifecycle_role_base + 2U) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           1U << 9U);
        (void)sys::ipc_call(endpoint, 0x44455354U, 0U);
        return 7;
    }
    if (role == lifecycle_role_base + 3U) {
        const auto request = sys::ipc_receive(endpoint);
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            return 8;
        sys::thread_exit(0U, notification, 1U << 10U);
    }
    if (role == lifecycle_role_base + 4U) {
        const auto result = sys::ipc_call(endpoint, 0x45584954U, 0U);
        if (result.status != static_cast<sys::word_t>(sys::error_t::timed_out))
            return 9;
        sys::thread_exit(0U, notification, 1U << 11U);
    }
    if (role == capability_race_server_role) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           1U << 12U);
        const auto request = sys::ipc_receive(endpoint);
        if (request.status == static_cast<sys::word_t>(sys::error_t::success))
            (void)sys::ipc_reply(0U, 0U, 0U, 0U);
        sys::thread_exit(0U, notification, 1U << 14U);
    }
    if (role == capability_race_sender_role) {
        while (sys::control(sys::abi::v1::control_operation::notification_signal, 20U, 1U) !=
               static_cast<sys::word_t>(sys::error_t::success)) {
        }
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           1U << 13U);
        sys::abi::v1::ipc_transfer transfer{};
        transfer.source = 20U;
        transfer.destination = 20U;
        transfer.rights = 1U << 1U;
        const auto result = sys::ipc_call(endpoint, 0x43415052U, 0U, 0U, 0U, transfer);
        if (result.status != static_cast<sys::word_t>(sys::error_t::success) &&
            result.status != static_cast<sys::word_t>(sys::error_t::not_found))
            return 10;
        sys::thread_exit(0U, notification, 1U << 15U);
    }
    if (role == object_race_worker_role) {
        while (sys::control(sys::abi::v1::control_operation::notification_signal, 20U, 1U) !=
               static_cast<sys::word_t>(sys::error_t::success)) {
        }
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           1U << 12U);
        while (sys::control(sys::abi::v1::control_operation::notification_signal, 20U, 1U) ==
               static_cast<sys::word_t>(sys::error_t::success)) {
        }
        sys::thread_exit(0U, notification, 1U << 13U);
    }
    if (role == undefined_instruction_role) {
#if defined(__aarch64__)
        asm volatile(".inst 0x00000000");
#endif
        return 11;
    }
    const sys::word_t client_index = role - 0x101U;
    if (client_index >= 2U)
        return 1;
    volatile sys::word_t* page = reinterpret_cast<volatile sys::word_t*>(fault_address);
    const sys::word_t value = 0x5a494c4300000000ULL | client_index;
    *page = value;
    if (*page != value)
        return 2;
    const sys::word_t call =
        sys_ipc_invoke_raw(endpoint, static_cast<sys::word_t>(sys::abi::v1::ipc_operation::call),
                           completion_magic, fault_address, 1U << client_index, 0U, 0U, 0U);
    return call == static_cast<sys::word_t>(sys::error_t::success) ? 0 : 3;
}
