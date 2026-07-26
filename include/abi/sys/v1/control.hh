#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class control_operation : word_t
    {
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
        acceptance_report = 14U,
        acceptance_finalize = 15U,
    };
}
