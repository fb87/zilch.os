#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class control_operation : word_t {
        capability_copy = 0U,
        capability_move = 1U,
        capability_delete = 2U,
        capability_revoke = 3U,
        thread_start = 4U,
        thread_suspend = 5U,
        thread_resume = 6U,
        map_frame = 7U,
        unmap_frame = 8U,
        notification_signal = 9U,
        notification_poll = 10U,
        interrupt_bind = 11U,
        interrupt_ack = 12U,
        scheduling_configure = 13U,
        capability_mint = 14U,
        child_create = 16U,
        child_destroy = 17U,
        hypervisor_invoke = 20U,
#if CONFIG_SELFTEST
        acceptance_report = 0x80000000U,
        acceptance_finalize = 0x80000001U,
        acceptance_worker_tick = 0x80000002U,
        acceptance_query = 0x80000003U,
#endif
#if CONFIG_HYPERVISOR_SELFTEST
        hypervisor_self_test = 0x80000100U,
#endif
    };
} // namespace sys::abi::v1
