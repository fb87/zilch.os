#pragma once

#include <sys/types.hh>

namespace sys::test_abi::v1
{
    enum class control_operation : word_t
    {
        acceptance_report = 0x80000000U,
        acceptance_finalize = 0x80000001U,
        acceptance_worker_tick = 0x80000002U,
        acceptance_query = 0x80000003U,
        hypervisor_self_test = 0x80000100U,
    };

    enum class hypervisor_operation : word_t
    {
        fuzz = 0x80000000U,
    };
}
