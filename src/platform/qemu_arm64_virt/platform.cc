#include <sys/kernel/init/init.hh>
#include <sys/platform/platform.hh>
#include <sys/platform/v1/timer_ops.hh>

extern "C" void sys_platform_link_anchor() noexcept {}

namespace
{
    sys::error_t timer_percpu_init(const sys::kernel::init::boot_context_t*) noexcept {
        sys::platform::timer::initialize();
        return sys::error_t::success;
    }
} // namespace

PLAT_INIT(timer, timer_percpu_init);

extern "C" const sys::platform::v1::timer_ops_t sys_platform_timer_ops = {
    .ticks_per_second = sys::platform::timer::ticks_per_second,
    .initialize = &sys::platform::timer::initialize,
    .ticks = &sys::platform::timer::ticks,
    .certification_valid = &sys::platform::timer::certification_valid,
};
