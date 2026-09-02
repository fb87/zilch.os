#pragma once

#include <sys/control.hh>
#include <sys/syscall.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace sys
{
    /*
     * Physical address backing a frame the caller owns. Requires the frame's
     * control right -- see abi::v1::control_operation::frame_physical_address.
     * Userspace DMA drivers need this because devices are programmed with
     * physical addresses; a virtqueue's rings are useless to a device
     * described by the driver's own virtual addresses.
     */
    [[nodiscard]] inline error_t frame_physical_address(capability_id_t frame,
                                                        word_t& address) noexcept {
        address = 0U;
        const word_t status =
            control_result1(address, abi::v1::control_operation::frame_physical_address, frame);
        return static_cast<error_t>(static_cast<s64>(status));
    }
} // namespace sys
