#include <sys/control_plane.hh>
#include <sys/kernel/capability.hh>
#include <sys/kernel/scheduling/context.hh>

namespace
{
    bool capability_properties() {
        using namespace sys::kernel::capability;
        constexpr rights_t full{static_cast<sys::u32>(right_t::read) |
                                static_cast<sys::u32>(right_t::write) |
                                static_cast<sys::u32>(right_t::grant)};
        for (sys::u32 bits = 0U; bits < 64U; ++bits) {
            const rights_t candidate{bits};
            const bool expected = bits != 0U && (bits & ~full.bits) == 0U;
            if (attenuates(full, candidate) != expected)
                return false;
        }
        return validate(slot_t{}, right_t::read) == sys::error_t::not_found;
    }

    bool scheduling_properties() {
        using namespace sys::kernel::scheduling;
        context value{};
        initialize(value, 0U);
        if (configure(value, 200U, 8U, 16U, 0U, 0U) != sys::error_t::success)
            return false;

        sys::u64 state = 0x9e3779b97f4a7c15ULL;
        sys::u64 now{};
        for (sys::u32 operation = 0U; operation < 65536U; ++operation) {
            state ^= state << 13U;
            state ^= state >> 7U;
            state ^= state << 17U;
            now += (state & 3U);
            if ((state & 7U) == 0U)
                replenish_if_due(value, now);
            else
                (void)charge(value, now, 1U);
            if (value.consumed_ticks > value.budget_ticks ||
                value.replenishment_count > maximum_replenishments ||
                value.donation_depth > maximum_donation_depth)
                return false;
            for (sys::u32 index = 1U; index < value.replenishment_count; ++index)
                if (value.replenishment_deadlines[index] <
                    value.replenishment_deadlines[index - 1U])
                    return false;
        }

        context donor{};
        context receiver{};
        initialize(donor, 0U);
        initialize(receiver, 0U);
        if (configure(donor, 220U, 8U, 16U, 0U, now) != sys::error_t::success ||
            configure(receiver, 20U, 4U, 16U, 0U, now) != sys::error_t::success ||
            donate(receiver, donor, now) != sys::error_t::success ||
            receiver.effective_priority != donor.effective_priority)
            return false;
        revoke_donation(receiver, donor);
        return receiver.effective_priority == receiver.priority && receiver.donation_depth == 0U;
    }

    bool control_plane_properties() {
        const sys::word_t first =
            static_cast<sys::word_t>(sys::abi::v1::control_plane_role::process);
        sys::word_t readiness = 0U;
        for (sys::word_t index = 0U; index < sys::abi::v1::control_plane_role_count; ++index) {
            const sys::word_t role = first + index;
            const auto policy = sys::control_plane::policy_for(role);
            if (!sys::control_plane::valid(policy))
                return false;
            if (policy.restart_limit != 0U &&
                (!sys::control_plane::may_restart(policy, 0U) ||
                 sys::control_plane::may_restart(policy, policy.restart_limit)))
                return false;
            const sys::word_t badge = sys::abi::v1::control_plane_ready_badge(role);
            const sys::word_t exit_badge = sys::abi::v1::control_plane_exit_badge(role);
            if (badge == 0U || exit_badge == 0U || (readiness & badge) != 0U ||
                (readiness & exit_badge) != 0U)
                return false;
            readiness |= badge;
        }
        const auto invalid = sys::control_plane::policy_for(first - 1U);
        return !sys::control_plane::valid(invalid) &&
               sys::abi::v1::control_plane_ready_badge(first - 1U) == 0U &&
               (readiness & sys::abi::v1::memory_service_ready_badge) == 0U &&
               readiness == (1U << sys::abi::v1::control_plane_role_count) - 1U;
    }
} // namespace

int main() {
    return capability_properties() && scheduling_properties() && control_plane_properties() ? 0 : 1;
}
