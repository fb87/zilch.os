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
} // namespace

int main() {
    return capability_properties() && scheduling_properties() ? 0 : 1;
}
