#pragma once

#include <sys/types.hh>

namespace sys::test_abi::v1
{
    enum class control_operation : word_t {
        acceptance_report = 0x80000000U,
        acceptance_finalize = 0x80000001U,
        acceptance_worker_tick = 0x80000002U,
        acceptance_query = 0x80000003U,
        memory_inject_extent_failure = 0x80000004U,
        memory_invariant_snapshot = 0x80000005U,
        memory_server_protocol_detail = 0x80000006U,
        hypervisor_self_test = 0x80000100U,
    };

    enum class hypervisor_operation : word_t {
        fuzz = 0x80000000U,
    };

    enum class fuzz_case : word_t {
        valid_call = 0U,
        invalid_capability = 1U,
        invalid_operation = 2U,
        wrong_thread_identity = 3U,
        random_payload = 4U,
        boundary_capability = 5U,
        mixed = 6U,
    };

    inline constexpr capability_id_t debug_endpoint = 1U;
    inline constexpr capability_id_t fuzz_endpoint = 2U;
    inline constexpr word_t fuzz_magic = 0x5a46555a5aULL;
} // namespace sys::test_abi::v1
