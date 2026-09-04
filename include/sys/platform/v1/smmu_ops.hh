#pragma once

#include <sys/kernel/init/init.hh>
#include <sys/types.hh>

/*
 * Driver-section pilot, extending the pattern validated for timer in
 * docs/architecture/KERNEL_ARCH_PLATFORM_DECOUPLING.md to a second
 * required driver slot.
 *
 * This covers discovery only (readiness checklist DEV-006: "ARM SMMU
 * discovery implemented"): identifying whether an SMMUv3 is present and
 * reading its capability registers. It does not enable translation
 * (CR0.SMMUEN is never touched) and does not implement stream tables,
 * command/event queues, per-domain translation contexts, invalidation,
 * or DMA-to-capability binding (DEV-007..018) -- those remain open per
 * docs/readiness/PRODUCTION_READINESS_CHECKLIST.md section 9, and per
 * docs/readiness/KERNEL_SECURITY_MODEL.md, DMA isolation before this
 * subsystem exists is outside the certified boundary. A platform with no
 * SMMU/IOMMU support at all (present() always false) is a fully valid,
 * honestly-reported implementation of this contract, not an error.
 */
namespace sys::platform::v1
{
    struct smmu_ops_t {
        bool (*present)() noexcept;
        u32 (*idr0)() noexcept;
        u32 (*idr1)() noexcept;
        bool (*stage1_supported)() noexcept;
        bool (*stage2_supported)() noexcept;
        u32 (*stream_id_bits)() noexcept;
    };
} // namespace sys::platform::v1

extern "C" SYS_OPS_DECL const sys::platform::v1::smmu_ops_t sys_platform_smmu_ops;
