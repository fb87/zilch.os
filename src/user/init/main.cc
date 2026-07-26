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
    };

    [[nodiscard]] bool report(test_id id, bool pass) noexcept
    {
        return sys::control(sys::abi::v1::control_operation::acceptance_report,
                            static_cast<sys::word_t>(id), pass ? 1U : 0U)
            == static_cast<sys::word_t>(sys::error_t::success);
    }
}

extern "C" int main() noexcept
{
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

    (void)sys::control(
        sys::abi::v1::control_operation::acceptance_finalize,
        pass ? 1U : 0U);
    return pass ? 0 : 1;
}
