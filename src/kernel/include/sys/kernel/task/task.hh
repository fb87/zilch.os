#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::task
{
    struct task {
        object::header_t object{};
        capability::cspace_t cspace{};
        space_id_t address_space_id{};
        capability_id_t fault_endpoint{};
        bool root{};
    };

    inline void initialize(task& value, space_id_t address_space_id) noexcept {
        capability::initialize(value.cspace);
        value.address_space_id = address_space_id;
        value.fault_endpoint = 0U;
        value.root = false;
    }
} // namespace sys::kernel::task
