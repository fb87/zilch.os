#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/printk.hh>
#include <sys/platform/v1/earlyfs.hh>

namespace sys::kernel::tests::earlyfs
{
    /*
     * Exercises the root-driven path-lookup mechanism end to end: create a
     * read-only earlyfs frame capability the way a real root task would,
     * confirm out-of-range pages are rejected, resolve a real entry's
     * (offset, size) the way root's own directory scan would, bind it to a
     * reserved test-only role, confirm image_for_role() now prefers the
     * binding over the historical hardcoded switch, and confirm a
     * dishonest (offset, size) claim is rejected regardless of what the
     * caller asserts -- root_graph.hh does not call any of this yet (see
     * bind_role_image()'s comment), so this is the only exerciser of the
     * mechanism today.
     */
    [[nodiscard]] inline error_t run_role_binding_check(task::task& root) noexcept {
        if (arch::space::earlyfs_image_size() == 0U) {
            // No earlyfs consumer on this arch (amd64) -- nothing to exercise.
            pr_info("[TEST] name=root_driven_image_lookup result=PASS skipped=1\n");
            return error_t::success;
        }

        constexpr capability_id_t test_frame_slot = 200U;
        constexpr word_t test_role = 0x7ffffffeU; // reserved, cannot collide with a real role

        if (memory::create_earlyfs_frame(root, test_frame_slot, 0U) != error_t::success)
            return error_t::invalid_argument;
        const usize_t page_count =
            (arch::space::earlyfs_image_size() + memory::page_size - 1U) / memory::page_size;
        if (memory::create_earlyfs_frame(root, test_frame_slot + 1U, page_count) !=
            error_t::denied) {
            (void)memory::destroy_frame(root, test_frame_slot);
            return error_t::invalid_argument;
        }
        if (memory::destroy_frame(root, test_frame_slot) != error_t::success)
            return error_t::invalid_argument;

        paddr_t page0 = 0U;
        if (!arch::space::earlyfs_page_address(0U, page0))
            return error_t::invalid_argument;
        const auto* bytes = reinterpret_cast<const u8*>(static_cast<uintptr_t>(page0));
        const auto found =
            platform::v1::earlyfs::find(bytes, arch::space::earlyfs_image_size(),
                                        "bin/memory-server");
        if (!found.valid())
            return error_t::invalid_argument;
        const u64 offset = static_cast<u64>(found.data - bytes);
        const u64 size = static_cast<u64>(found.size);

        if (arch::space::bind_role_image(test_role, offset,
                                         static_cast<u64>(arch::space::earlyfs_image_size()) -
                                             offset + 1U) == error_t::success)
            return error_t::invalid_argument; // an over-claimed size must be rejected

        if (arch::space::bind_role_image(test_role, offset, size) != error_t::success)
            return error_t::invalid_argument;

        const arch::space::image_view bound = arch::space::image_for_role(test_role);
        if (static_cast<usize_t>(bound.end - bound.start) != size ||
            reinterpret_cast<uintptr_t>(bound.start) != page0 + offset)
            return error_t::invalid_argument;

        pr_info("[TEST] name=root_driven_image_lookup result=PASS role=%llx size=%llu\n",
                static_cast<unsigned long long>(test_role), static_cast<unsigned long long>(size));
        return error_t::success;
    }
} // namespace sys::kernel::tests::earlyfs
