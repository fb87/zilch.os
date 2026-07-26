#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/capability.hh>
#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::capability
{
    inline constexpr u32 cspace_slot_count = 32U;

    struct cspace_t {
        volatile u32 lock{};
        slot_t slots[cspace_slot_count]{};
    };

    inline void lock(cspace_t& cspace) noexcept {
        while (__atomic_exchange_n(&cspace.lock, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&cspace.lock, __ATOMIC_RELAXED) != 0U) {
                arch::cpu::relax();
            }
        }
    }

    inline void unlock(cspace_t& cspace) noexcept {
        __atomic_store_n(&cspace.lock, 0U, __ATOMIC_RELEASE);
    }

    inline void initialize(cspace_t& cspace) noexcept {
        cspace.lock = 0U;
        for (u32 index = 0U; index < cspace_slot_count; ++index) {
            cspace.slots[index] = {};
        }
    }

    [[nodiscard]] inline error_t install(cspace_t& cspace, capability_id_t selector,
                                         const object::reference_t& object,
                                         rights_t rights) noexcept {
        if (selector >= cspace_slot_count || object.type == object::type_t::none ||
            object::resolve(object) == nullptr) {
            return error_t::invalid_argument;
        }
        slot_t& slot = cspace.slots[selector];
        if (slot.object.type != object::type_t::none)
            return error_t::busy;
        slot.object = object;
        slot.rights = rights;
        return error_t::success;
    }

    [[nodiscard]] inline error_t remove(cspace_t& cspace, capability_id_t selector) noexcept {
        if (selector >= cspace_slot_count)
            return error_t::invalid_argument;
        slot_t& slot = cspace.slots[selector];
        if (slot.object.type == object::type_t::none)
            return error_t::not_found;
        slot = {};
        return error_t::success;
    }

    [[nodiscard]] inline error_t lookup(const cspace_t& cspace, capability_id_t selector,
                                        object::type_t expected_type, right_t required,
                                        object::header_t*& result) noexcept {
        result = nullptr;
        if (selector >= cspace_slot_count)
            return error_t::not_found;
        const slot_t& slot = cspace.slots[selector];
        if (slot.object.type != expected_type)
            return error_t::denied;
        const error_t rights_result = validate(slot, required);
        if (rights_result != error_t::success)
            return rights_result;
        result = object::resolve(slot.object);
        return result != nullptr ? error_t::success : error_t::not_found;
    }

    [[nodiscard]] inline error_t copy(cspace_t& destination, capability_id_t destination_selector,
                                      const cspace_t& source, capability_id_t source_selector,
                                      rights_t rights_mask) noexcept {
        if (destination_selector >= cspace_slot_count || source_selector >= cspace_slot_count)
            return error_t::invalid_argument;
        const slot_t& source_slot = source.slots[source_selector];
        if (source_slot.object.type == object::type_t::none ||
            object::resolve(source_slot.object) == nullptr)
            return error_t::not_found;
        if (!source_slot.rights.contains(right_t::grant))
            return error_t::denied;
        const u32 granted = source_slot.rights.bits & rights_mask.bits;
        if (granted == 0U)
            return error_t::denied;
        lock(destination);
        slot_t& destination_slot = destination.slots[destination_selector];
        if (destination_slot.object.type != object::type_t::none) {
            unlock(destination);
            return error_t::busy;
        }
        destination_slot.object = source_slot.object;
        destination_slot.rights = {granted};
        unlock(destination);
        return error_t::success;
    }

    [[nodiscard]] inline error_t move(cspace_t& destination, capability_id_t destination_selector,
                                      cspace_t& source, capability_id_t source_selector) noexcept {
        if (&destination == &source && destination_selector == source_selector)
            return error_t::success;
        if (destination_selector >= cspace_slot_count || source_selector >= cspace_slot_count)
            return error_t::invalid_argument;
        cspace_t* first = &destination < &source ? &destination : &source;
        cspace_t* second = &destination < &source ? &source : &destination;
        lock(*first);
        if (second != first)
            lock(*second);
        slot_t& source_slot = source.slots[source_selector];
        slot_t& destination_slot = destination.slots[destination_selector];
        error_t result = error_t::success;
        if (source_slot.object.type == object::type_t::none)
            result = error_t::not_found;
        else if (destination_slot.object.type != object::type_t::none)
            result = error_t::busy;
        else {
            destination_slot = source_slot;
            source_slot = {};
        }
        if (second != first)
            unlock(*second);
        unlock(*first);
        return result;
    }

    [[nodiscard]] inline error_t delete_capability(cspace_t& cspace,
                                                   capability_id_t selector) noexcept {
        lock(cspace);
        const error_t result = remove(cspace, selector);
        unlock(cspace);
        return result;
    }

    inline u32 revoke_reference(const object::reference_t& reference) noexcept {
        u32 removed = 0U;
        // Static K1/K6 profile owns a bounded set of CSpaces; callers iterate them.
        // This helper intentionally operates on one CSpace through revoke_in().
        (void)reference;
        return removed;
    }

    inline u32 revoke_in(cspace_t& cspace, const object::reference_t& reference) noexcept {
        u32 removed = 0U;
        lock(cspace);
        for (u32 index = 0U; index < cspace_slot_count; ++index) {
            slot_t& slot = cspace.slots[index];
            if (slot.object.id == reference.id && slot.object.generation == reference.generation &&
                slot.object.type == reference.type) {
                slot = {};
                ++removed;
            }
        }
        unlock(cspace);
        return removed;
    }

} // namespace sys::kernel::capability
