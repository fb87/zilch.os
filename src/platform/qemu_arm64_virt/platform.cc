#include <sys/kernel/init/init.hh>
#include <sys/kernel/printk.hh>
#include <sys/platform/platform.hh>
#include <sys/platform/smmu.hh>
#include <sys/platform/v1/smmu_ops.hh>
#include <sys/platform/v1/timer_ops.hh>

extern "C" void sys_platform_link_anchor() noexcept {}

namespace
{
    sys::error_t timer_percpu_init(const sys::kernel::init::boot_context_t*) noexcept {
        sys::platform::timer::initialize();
        return sys::error_t::success;
    }

    sys::error_t smmu_init(const sys::kernel::init::boot_context_t*) noexcept {
        sys::platform::smmu::initialize();
        if (sys::platform::smmu::present()) {
            pr_info("smmu: present idr0=%08x idr1=%08x stage1=%u stage2=%u sid_bits=%u "
                    "(discovery only -- translation not enabled)\n",
                    static_cast<unsigned int>(sys::platform::smmu::idr0()),
                    static_cast<unsigned int>(sys::platform::smmu::idr1()),
                    static_cast<unsigned int>(sys::platform::smmu::stage1_supported()),
                    static_cast<unsigned int>(sys::platform::smmu::stage2_supported()),
                    static_cast<unsigned int>(sys::platform::smmu::stream_id_bits()));
        } else {
            pr_info("smmu: not present (boot without iommu=smmuv3, or unsupported)\n");
        }
        return sys::error_t::success; // absence is never fatal -- see DEV-GATE
    }
} // namespace

PLAT_INIT(timer, timer_percpu_init);
PLAT_INIT(smmu, smmu_init);

// Named section + KEEP() in kernel.ld, not just `used`: `used` only stops
// the compiler from treating this as dead code -- it does not stop
// --gc-sections from dropping an unreferenced section at link time
// (confirmed empirically while building this pilot: sys_platform_smmu_ops
// vanished from the symbol table under `used` alone, since nothing calls
// it yet). Placing it in its own named section and KEEP()-ing that
// section is the same mechanism already proven for .sys_init_timer.
extern "C" __attribute__((used, section(".sys_ops_timer")))
const sys::platform::v1::timer_ops_t sys_platform_timer_ops = {
    .ticks_per_second = sys::platform::timer::ticks_per_second,
    .initialize = &sys::platform::timer::initialize,
    .ticks = &sys::platform::timer::ticks,
    .certification_valid = &sys::platform::timer::certification_valid,
};

// The ops contract is a stable platform ABI surface meant to outlive
// whether today's kernel snapshot happens to call it -- see the note on
// sys_platform_timer_ops above for why this needs a named KEEP()'d
// section rather than `used` alone.
extern "C" __attribute__((used, section(".sys_ops_smmu")))
const sys::platform::v1::smmu_ops_t sys_platform_smmu_ops = {
    .present = &sys::platform::smmu::present,
    .idr0 = &sys::platform::smmu::idr0,
    .idr1 = &sys::platform::smmu::idr1,
    .stage1_supported = &sys::platform::smmu::stage1_supported,
    .stage2_supported = &sys::platform::smmu::stage2_supported,
    .stream_id_bits = &sys::platform::smmu::stream_id_bits,
};
