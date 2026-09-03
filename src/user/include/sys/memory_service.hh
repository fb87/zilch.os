#pragma once

#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace sys::memory_service
{
    inline constexpr word_t notification = native::root_notification;
    inline constexpr word_t resource = native::memory_resource;
    inline constexpr word_t service_frame_base = 16U;

    struct handle_state final {
        bool allocated{};
        word_t owner{};
        capability_id_t destination{};
    };

    inline handle_state handles[abi::v1::memory_server_max_handles]{};

    [[nodiscard]] inline word_t service_request(const abi::v1::ipc_result& request, word_t& value,
                                                abi::v1::ipc_transfer& transfer) noexcept {
        const auto operation = static_cast<abi::v1::memory_server_operation>(request.message0);
        const word_t handle = request.message1;
        const auto destination = static_cast<capability_id_t>(request.message2);
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t invalid = static_cast<word_t>(error_t::invalid_argument);
        const word_t denied = static_cast<word_t>(error_t::denied);

        switch (operation) {
            case abi::v1::memory_server_operation::allocate_frame:
            case abi::v1::memory_server_operation::grant_frame:
                if (handle >= abi::v1::memory_server_max_handles || handles[handle].allocated)
                    return invalid;
                if (control(abi::v1::control_operation::resource_frame_create, resource,
                            service_frame_base + handle) != success)
                    return static_cast<word_t>(error_t::no_memory);
                handles[handle].allocated = true;
                handles[handle].owner = request.sender;
                handles[handle].destination = destination;
                transfer.source = static_cast<capability_id_t>(service_frame_base + handle);
                transfer.destination = destination;
                transfer.rights = static_cast<u32>(abi::v1::CapabilityRight::read) |
                                  static_cast<u32>(abi::v1::CapabilityRight::write);
                value = handle;
                return success;

            case abi::v1::memory_server_operation::release_frame:
                if (handle >= abi::v1::memory_server_max_handles || !handles[handle].allocated)
                    return invalid;
                if (handles[handle].owner != request.sender)
                    return denied;
                if (control(abi::v1::control_operation::frame_destroy,
                            service_frame_base + handle) != success)
                    return static_cast<word_t>(error_t::busy);
                handles[handle] = {};
                value = handle;
                return success;

            case abi::v1::memory_server_operation::query:
                return control_result1(value, abi::v1::control_operation::memory_resource_query,
                                       resource);

            case abi::v1::memory_server_operation::shutdown:
                for (word_t index = 0U; index < abi::v1::memory_server_max_handles; ++index) {
                    if (handles[index].allocated && handles[index].owner == request.sender)
                        return static_cast<word_t>(error_t::busy);
                }
                return success;
        }
        return invalid;
    }
} // namespace sys::memory_service
