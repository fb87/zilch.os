#pragma once

#include <sys/certification.hh>
#include <sys/control.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>
#include <sys/test_abi/v1/certification.hh>

/*
 * Memory-subsystem certification cases, lifted out of src/user/init/main.cc
 * so that file stays within the 1200-line source boundary. These six are
 * self-contained: unlike the process-lifecycle and IPC-race cases that
 * remain in main.cc, none of them needs the harness's shared scaffolding
 * (create/destroy_service_process, wait_for_badges, the acceptance ledger),
 * so they move without the callback plumbing control_plane_certification.hh
 * needs for its own extraction.
 *
 * Each returns pass/fail; main.cc still owns recording them in the ledger,
 * so test identity and ordering stay in one place.
 */
namespace sys::memory_certification
{
    [[nodiscard]] inline bool resource_delegation() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t child_resource_selector = 33U;
        constexpr sys::word_t first_frame = 34U;
        constexpr sys::word_t second_frame = 35U;
        constexpr sys::word_t third_frame = 36U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t no_memory = static_cast<sys::word_t>(sys::error_t::no_memory);

        if (sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                         root_task_selector, root_resource_selector, child_resource_selector,
                         2U) != success)
            return false;
        sys::word_t used = 0U;
        if (sys::control_result1(used, sys::abi::v1::control_operation::memory_resource_query,
                                 child_resource_selector) != success ||
            used != 0U)
            return false;
        bool passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                                   child_resource_selector, first_frame) == success;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource_selector, second_frame) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource_selector, third_frame) == no_memory &&
                 passed;
        if (sys::control_result1(used, sys::abi::v1::control_operation::memory_resource_query,
                                 child_resource_selector) != success ||
            used != 2U)
            passed = false;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, first_frame) == success &&
            passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, second_frame) == success &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              child_resource_selector) == success &&
                 passed;
        return passed;
    }

    [[nodiscard]] inline bool extent_retype() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t parent_resource = 37U;
        constexpr sys::word_t child_resource = 38U;
        constexpr sys::word_t parent_frame0 = 39U;
        constexpr sys::word_t parent_frame1 = 40U;
        constexpr sys::word_t parent_frame2 = 41U;
        constexpr sys::word_t child_frame0 = 42U;
        constexpr sys::word_t child_frame1 = 43U;
        constexpr sys::word_t child_frame2 = 44U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t no_memory = static_cast<sys::word_t>(sys::error_t::no_memory);

        bool passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                   root_task_selector, root_resource_selector, parent_resource,
                                   4U) == success;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                              root_task_selector, parent_resource, child_resource, 2U) == success &&
                 passed;

        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              parent_resource, parent_frame0) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              parent_resource, parent_frame1) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              parent_resource, parent_frame2) == no_memory &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource, child_frame0) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource, child_frame1) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                              child_resource, child_frame2) == no_memory &&
                 passed;

        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, child_frame0) == success &&
            passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, child_frame1) == success &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              child_resource) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, parent_frame0) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, parent_frame1) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              parent_resource) == success &&
                 passed;
        return passed;
    }

    [[nodiscard]] inline bool extent_metadata() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t parent_resource = 33U;
        constexpr sys::word_t first_child = 34U;
        constexpr sys::word_t child_count = 20U;
        constexpr sys::word_t frame_selector = 54U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        bool passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                   root_task_selector, root_resource_selector, parent_resource,
                                   child_count) == success;
        for (sys::word_t index = 0U; index < child_count && passed; ++index) {
            passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                  root_task_selector, parent_resource, first_child + index,
                                  1U) == success;
        }

        /* Return alternating extents first to force a fragmented parent list. */
        for (sys::word_t parity = 0U; parity < 2U && passed; ++parity) {
            for (sys::word_t index = parity; index < child_count; index += 2U) {
                passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                                      first_child + index) == success &&
                         passed;
            }
        }

        /* A full-size redelegation proves deterministic merge and node reuse. */
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                              root_task_selector, parent_resource, first_child,
                              child_count) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::resource_frame_create, first_child,
                              frame_selector) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              first_child) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              parent_resource) == success &&
                 passed;
        return passed;
    }

    [[nodiscard]] inline bool pressure_rollback() noexcept {
        constexpr sys::word_t root_task_selector = 1U;
        constexpr sys::word_t root_resource_selector = 32U;
        constexpr sys::word_t parent_resource = 33U;
        constexpr sys::word_t child_resource = 34U;
        constexpr sys::word_t first_frame = 35U;
        constexpr sys::word_t frame_count = 16U;
        constexpr sys::word_t overflow_frame = first_frame + frame_count;
        constexpr sys::word_t cycles = 32U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t no_memory = static_cast<sys::word_t>(sys::error_t::no_memory);

        sys::word_t before = 0U;
        if (sys::certification::control_result1(
                before, sys::test_abi::v1::control_operation::memory_invariant_snapshot) != success)
            return false;

        /* Force the split-node allocation to fail and prove full rollback. */
        bool passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                   root_task_selector, root_resource_selector, parent_resource,
                                   8U) == success;
        passed = sys::certification::control(
                     sys::test_abi::v1::control_operation::memory_inject_extent_failure, 1U) ==
                     success &&
                 passed;
        passed =
            sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                         root_task_selector, parent_resource, child_resource, 1U) == no_memory &&
            passed;
        passed = sys::certification::control(
                     sys::test_abi::v1::control_operation::memory_inject_extent_failure, 0U) ==
                     success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                              parent_resource) == success &&
                 passed;

        /* Repeatedly drive one resource to its quota and reclaim every page. */
        for (sys::word_t cycle = 0U; cycle < cycles && passed; ++cycle) {
            passed = sys::control(sys::abi::v1::control_operation::memory_resource_delegate,
                                  root_task_selector, root_resource_selector, parent_resource,
                                  frame_count) == success;
            for (sys::word_t index = 0U; index < frame_count && passed; ++index) {
                passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                                      parent_resource, first_frame + index) == success;
            }
            passed = sys::control(sys::abi::v1::control_operation::resource_frame_create,
                                  parent_resource, overflow_frame) == no_memory &&
                     passed;
            for (sys::word_t index = 0U; index < frame_count; ++index) {
                passed = sys::control(sys::abi::v1::control_operation::frame_destroy,
                                      first_frame + index) == success &&
                         passed;
            }
            passed = sys::control(sys::abi::v1::control_operation::memory_resource_destroy,
                                  parent_resource) == success &&
                     passed;
        }

        sys::word_t after = 0U;
        passed = sys::certification::control_result1(
                     after, sys::test_abi::v1::control_operation::memory_invariant_snapshot) ==
                     success &&
                 passed;
        return passed && before == after;
    }

    [[nodiscard]] inline bool resource_lifecycle() noexcept {
        constexpr sys::word_t first_frame = 16U;
        constexpr sys::word_t frame_total = 8U;
        constexpr sys::word_t first_table = 24U;
        constexpr sys::word_t table_total = 4U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);

        sys::word_t owned_before = 0U;
        if (sys::control_result1(owned_before, sys::abi::v1::control_operation::memory_query) !=
            success)
            return false;

        for (sys::word_t index = 0U; index < frame_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::frame_create, 0U,
                             first_frame + index) != success)
                return false;
        }
        for (sys::word_t index = 0U; index < table_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::page_table_create, 0U,
                             first_table + index, 3U) != success)
                return false;
        }

        sys::word_t owned_peak = 0U;
        if (sys::control_result1(owned_peak, sys::abi::v1::control_operation::memory_query) !=
                success ||
            owned_peak != owned_before + frame_total + table_total)
            return false;

        for (sys::word_t index = 0U; index < table_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::page_table_destroy,
                             first_table + index) != success)
                return false;
        }
        for (sys::word_t index = 0U; index < frame_total; ++index) {
            if (sys::control(sys::abi::v1::control_operation::frame_destroy, first_frame + index) !=
                success)
                return false;
        }

        sys::word_t owned_after = 0U;
        return sys::control_result1(owned_after, sys::abi::v1::control_operation::memory_query) ==
                   success &&
               owned_after == owned_before;
    }

    [[nodiscard]] inline bool mapping_database() noexcept {
        constexpr sys::word_t frame_selector = 16U;
        constexpr sys::word_t root_space_selector = 3U;
        constexpr sys::word_t first_address = 0x2000a000U;
        constexpr sys::word_t second_address = 0x2000b000U;
        constexpr sys::word_t read_write = 3U;
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t busy = static_cast<sys::word_t>(sys::error_t::busy);

        if (sys::control(sys::abi::v1::control_operation::frame_create, 0U, frame_selector) !=
            success)
            return false;

        bool passed = sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                                   frame_selector, first_address, read_write, 0x100U) == success;
        passed = sys::control(sys::abi::v1::control_operation::map_frame, root_space_selector,
                              frame_selector, second_address, read_write, 0x100U) == success &&
                 passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) == busy &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                              frame_selector, first_address) == success &&
                 passed;
        passed =
            sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) == busy &&
            passed;
        passed = sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                              frame_selector, second_address) == success &&
                 passed;
        passed = sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector) ==
                     success &&
                 passed;

        if (!passed) {
            (void)sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                               frame_selector, first_address);
            (void)sys::control(sys::abi::v1::control_operation::unmap_frame, root_space_selector,
                               frame_selector, second_address);
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy, frame_selector);
        }
        return passed;
    }
} // namespace sys::memory_certification
