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
        frame_allocate = 15U,
        frame_release = 18U,
        child_create = 16U,
        child_destroy = 17U,
        hypervisor_invoke = 20U,
        frame_create = 21U,
        frame_destroy = 22U,
        page_table_create = 23U,
        memory_set_quota = 24U,
        fault_reply_map = 25U,
        memory_query = 26U,
        page_table_destroy = 27U,
        process_create = 28U,
        process_destroy = 29U,
        fault_reply_sender = 30U,
        pager_reclaim_sender = 31U,
        endpoint_create = 32U,
        endpoint_destroy = 33U,
        notification_create = 34U,
        notification_destroy = 35U,
        device_frame_create = 36U,
        memory_resource_delegate = 37U,
        resource_frame_create = 38U,
        resource_page_table_create = 39U,
        memory_resource_query = 40U,
        memory_resource_destroy = 41U,
        thread_exit = 42U,
        interrupt_create = 43U,
        earlyfs_frame_create = 44U,
        role_image_bind = 45U,
        thread_create = 46U,
        /*
         * Reports the physical address backing a frame, in result1.
         * Requires the frame's control right, which a task creating its own
         * frame already holds (create_frame installs read|write|grant|
         * control) but which a merely-delegated read/write capability does
         * not -- so this does not widen what a frame handed to a client
         * discloses.
         *
         * Needed because DMA-capable devices are programmed with physical
         * addresses: a userspace driver building virtqueue rings (see
         * src/user/drivers/virtio) has no other way to tell a device where
         * its rings live. seL4_ARM_Page_GetAddress is the direct precedent.
         */
        frame_physical_address = 47U,
        /*
         * Reports whether the thread named by a1 has exited and, in
         * result1, the status it passed to thread_exit. Returns `busy` while
         * it is still running, so a caller polls rather than blocks -- the
         * same shape as notification_poll, and for the same reason: this
         * kernel has no blocking wait primitive, and adding a new blocking
         * state to the scheduler is a much larger change than a supervisor
         * looping on a bounded ipc_receive timeout, which is what every
         * other consumer here already does.
         *
         * Takes a thread capability rather than the thread id
         * process_create returns, so a stale reference fails closed through
         * the ordinary generation check instead of reading whichever
         * process later reused the slot.
         */
        process_wait = 48U,
        /*
         * Duplicates the caller into a new process. Returns success in both,
         * distinguished by result1: the child's thread id in the parent, and
         * zero in the child.
         *
         * The copy is eager -- no copy-on-write -- so it costs one physical
         * page per mapped page. Frame-backed mappings are duplicated as
         * private copies; device mappings are not, since a device frame is a
         * specific MMIO page and no private copy of one exists.
         */
        process_fork = 49U,
        /*
         * Replaces the caller's image with the one bound to role a1,
         * discarding its address space and restarting at the new entry
         * point. The cspace survives, which is what lets a forked child
         * arrange its capabilities and then exec.
         */
        process_exec = 50U,
    };
} // namespace sys::abi::v1
