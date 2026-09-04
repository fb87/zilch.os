#include <sys/kernel/init/init.hh>
#include <sys/kernel/printk.hh>
#include <sys/platform/platform.hh>
#include <sys/platform/v1/smmu_ops.hh>
#include <sys/platform/v1/timer_ops.hh>

extern "C" void sys_platform_link_anchor() noexcept {}

namespace
{
    sys::error_t timer_percpu_init(const sys::kernel::init::boot_context_t*) noexcept {
        sys::platform::timer::initialize();
        return sys::error_t::success;
    }

    // No IOMMU driver on this platform -- Intel VT-d (a distinct spec and
    // register model from ARM SMMUv3) is not implemented. Reporting
    // "not present" here is an honest, complete implementation of the
    // smmu_ops_t contract for a platform with no IOMMU support, not a
    // placeholder standing in for missing work.
    sys::error_t smmu_init(const sys::kernel::init::boot_context_t*) noexcept {
        pr_info("smmu: not present (no IOMMU driver on this platform)\n");
        return sys::error_t::success;
    }

    [[nodiscard]] bool smmu_absent() noexcept {
        return false;
    }

    [[nodiscard]] sys::u32 smmu_zero() noexcept {
        return 0U;
    }
} // namespace

PLAT_INIT(percpu, timer, timer_percpu_init);
PLAT_INIT(once, smmu, smmu_init);

extern "C" SYS_OPS const sys::platform::v1::timer_ops_t sys_platform_timer_ops = {
    .ticks_per_second = sys::platform::timer::ticks_per_second,
    .initialize = &sys::platform::timer::initialize,
    .ticks = &sys::platform::timer::ticks,
    .certification_valid = &sys::platform::timer::certification_valid,
};

extern "C" SYS_OPS const sys::platform::v1::smmu_ops_t sys_platform_smmu_ops = {
    .present = &smmu_absent,
    .idr0 = &smmu_zero,
    .idr1 = &smmu_zero,
    .stage1_supported = &smmu_absent,
    .stage2_supported = &smmu_absent,
    .stream_id_bits = &smmu_zero,
};
