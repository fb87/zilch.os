#pragma once

#include <sys/arch/user_access.hh>
#include <sys/types.hh>

namespace sys::kernel::user_access
{
    using arch::user_access::copy_from_user;
    using arch::user_access::copy_to_user;
    using arch::user_access::valid_range;
} // namespace sys::kernel::user_access
