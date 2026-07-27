#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/capability.hh>
#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::capability
{
    inline constexpr u32 cspace_slot_count = 64U;
    inline constexpr u32 maximum_registered_cspaces = 32U;
    inline constexpr u32 maximum_derivations = 4096U;
    inline constexpr u32 maximum_derivation_depth = 64U;

    struct cspace_t {
        volatile u32 lock{};
        u32 registry_index{maximum_registered_cspaces};
        slot_t slots[cspace_slot_count]{};
    };

    struct derivation_record_t {
        derivation_id_t id{};
        derivation_id_t parent{};
        object::reference_t object{};
        volatile u32 active{};
    };

    inline cspace_t* cspace_registry[maximum_registered_cspaces]{};
    inline volatile u32 cspace_registry_lock{};
    inline derivation_record_t derivations[maximum_derivations]{};
    inline volatile u32 derivation_lock{};
    inline volatile u32 authority_lock{};
    inline derivation_id_t next_derivation_hint{1U};

    inline void spin_lock(volatile u32& value) noexcept {
        while (__atomic_exchange_n(&value, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&value, __ATOMIC_RELAXED) != 0U) {
                arch::cpu::relax();
            }
        }
    }

    inline void spin_unlock(volatile u32& value) noexcept {
        __atomic_store_n(&value, 0U, __ATOMIC_RELEASE);
    }

    inline void lock(cspace_t& cspace) noexcept {
        spin_lock(cspace.lock);
    }
    inline void lock_authority() noexcept {
        spin_lock(authority_lock);
    }
    inline void unlock_authority() noexcept {
        spin_unlock(authority_lock);
    }
    inline void unlock(cspace_t& cspace) noexcept {
        spin_unlock(cspace.lock);
    }

    [[nodiscard]] inline derivation_id_t
    allocate_derivation(derivation_id_t parent, const object::reference_t& object) noexcept {
        spin_lock(derivation_lock);
        derivation_id_t candidate = next_derivation_hint;
        for (u32 scanned = 1U; scanned < maximum_derivations; ++scanned) {
            if (candidate == 0U || candidate >= maximum_derivations)
                candidate = 1U;
            derivation_record_t& record = derivations[candidate];
            if (__atomic_load_n(&record.active, __ATOMIC_ACQUIRE) == 0U) {
                record.id = candidate;
                record.parent = parent;
                record.object = object;
                __atomic_store_n(&record.active, 1U, __ATOMIC_RELEASE);
                next_derivation_hint = candidate + 1U;
                spin_unlock(derivation_lock);
                return candidate;
            }
            ++candidate;
        }
        spin_unlock(derivation_lock);
        return 0U;
    }

    [[nodiscard]] inline bool derivation_valid(derivation_id_t id,
                                               const object::reference_t& object) noexcept {
        if (id == 0U || id >= maximum_derivations)
            return false;
        const derivation_record_t& record = derivations[id];
        return __atomic_load_n(&record.active, __ATOMIC_ACQUIRE) != 0U && record.id == id &&
               record.object.id == object.id && record.object.generation == object.generation &&
               record.object.type == object.type;
    }

    [[nodiscard]] inline bool descendant_of(derivation_id_t candidate,
                                            derivation_id_t ancestor) noexcept {
        if (candidate == 0U || ancestor == 0U || candidate == ancestor)
            return false;
        for (u32 depth = 0U; depth < maximum_derivation_depth; ++depth) {
            if (candidate >= maximum_derivations)
                return false;
            const derivation_record_t& record = derivations[candidate];
            if (record.id != candidate || __atomic_load_n(&record.active, __ATOMIC_ACQUIRE) == 0U)
                return false;
            if (record.parent == ancestor)
                return true;
            if (record.parent == 0U || record.parent == candidate)
                return false;
            candidate = record.parent;
        }
        return false;
    }

    [[nodiscard]] inline error_t register_cspace(cspace_t& cspace) noexcept {
        spin_lock(cspace_registry_lock);
        for (u32 index = 0U; index < maximum_registered_cspaces; ++index) {
            if (cspace_registry[index] == &cspace) {
                cspace.registry_index = index;
                spin_unlock(cspace_registry_lock);
                return error_t::success;
            }
        }
        for (u32 index = 0U; index < maximum_registered_cspaces; ++index) {
            if (cspace_registry[index] == nullptr) {
                cspace_registry[index] = &cspace;
                cspace.registry_index = index;
                spin_unlock(cspace_registry_lock);
                return error_t::success;
            }
        }
        spin_unlock(cspace_registry_lock);
        return error_t::no_memory;
    }

    inline void initialize(cspace_t& cspace) noexcept {
        cspace.lock = 0U;
        cspace.registry_index = maximum_registered_cspaces;
        for (u32 index = 0U; index < cspace_slot_count; ++index)
            cspace.slots[index] = {};
        (void)register_cspace(cspace);
    }

    [[nodiscard]] inline error_t install(cspace_t& cspace, capability_id_t selector,
                                         const object::reference_t& object,
                                         rights_t granted_rights) noexcept {
        if (selector >= cspace_slot_count || object.type == object::type_t::none ||
            object::resolve(object) == nullptr || granted_rights.bits == 0U) {
            return error_t::invalid_argument;
        }
        lock(cspace);
        slot_t& slot = cspace.slots[selector];
        if (slot.object.type != object::type_t::none) {
            unlock(cspace);
            return error_t::busy;
        }
        const derivation_id_t derivation = allocate_derivation(0U, object);
        if (derivation == 0U) {
            unlock(cspace);
            return error_t::no_memory;
        }
        slot.object = object;
        slot.rights = granted_rights;
        slot.derivation = derivation;
        slot.parent = 0U;
        slot.badge = 0U;
        slot.depth = 0U;
        unlock(cspace);
        return error_t::success;
    }

    [[nodiscard]] inline error_t remove_locked(cspace_t& cspace,
                                               capability_id_t selector) noexcept {
        if (selector >= cspace_slot_count)
            return error_t::invalid_argument;
        slot_t& slot = cspace.slots[selector];
        if (slot.object.type == object::type_t::none)
            return error_t::not_found;
        if (slot.derivation < maximum_derivations)
            __atomic_store_n(&derivations[slot.derivation].active, 0U, __ATOMIC_RELEASE);
        slot = {};
        return error_t::success;
    }

    [[nodiscard]] inline error_t remove(cspace_t& cspace, capability_id_t selector) noexcept {
        return remove_locked(cspace, selector);
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
        if (!derivation_valid(slot.derivation, slot.object))
            return error_t::not_found;
        const error_t rights_result = validate(slot, required);
        if (rights_result != error_t::success)
            return rights_result;
        result = object::resolve(slot.object);
        return result != nullptr ? error_t::success : error_t::not_found;
    }

    [[nodiscard]] inline error_t lookup_slot(const cspace_t& cspace, capability_id_t selector,
                                             object::type_t expected_type, right_t required,
                                             slot_t& result) noexcept {
        result = {};
        if (selector >= cspace_slot_count)
            return error_t::not_found;
        cspace_t& mutable_cspace = const_cast<cspace_t&>(cspace);
        lock(mutable_cspace);
        const slot_t slot = cspace.slots[selector];
        unlock(mutable_cspace);
        if (slot.object.type != expected_type)
            return error_t::denied;
        if (!derivation_valid(slot.derivation, slot.object))
            return error_t::not_found;
        const error_t rights_result = validate(slot, required);
        if (rights_result != error_t::success)
            return rights_result;
        if (object::resolve(slot.object) == nullptr)
            return error_t::not_found;
        result = slot;
        return error_t::success;
    }
    [[nodiscard]] inline error_t derive(cspace_t& destination, capability_id_t destination_selector,
                                        const cspace_t& source, capability_id_t source_selector,
                                        rights_t rights_mask, badge_t badge) noexcept {
        if (destination_selector >= cspace_slot_count || source_selector >= cspace_slot_count)
            return error_t::invalid_argument;
        cspace_t& mutable_source = const_cast<cspace_t&>(source);
        cspace_t* first = &destination < &mutable_source ? &destination : &mutable_source;
        cspace_t* second = &destination < &mutable_source ? &mutable_source : &destination;
        lock(*first);
        if (second != first)
            lock(*second);
        const slot_t source_slot = source.slots[source_selector];
        slot_t& destination_slot = destination.slots[destination_selector];
        error_t result = error_t::success;
        if (source_slot.object.type == object::type_t::none ||
            object::resolve(source_slot.object) == nullptr) {
            result = error_t::not_found;
        } else if (!source_slot.rights.contains(right_t::grant) ||
                   !attenuates(source_slot.rights, rights_mask)) {
            result = error_t::denied;
        } else if (source_slot.depth >= maximum_derivation_depth) {
            result = error_t::invalid_argument;
        } else if (destination_slot.object.type != object::type_t::none) {
            result = error_t::busy;
        } else {
            const derivation_id_t id =
                allocate_derivation(source_slot.derivation, source_slot.object);
            if (id == 0U) {
                result = error_t::no_memory;
            } else {
                destination_slot.object = source_slot.object;
                destination_slot.rights = rights_mask;
                destination_slot.derivation = id;
                destination_slot.parent = source_slot.derivation;
                destination_slot.badge = badge;
                destination_slot.depth = source_slot.depth + 1U;
            }
        }
        if (second != first)
            unlock(*second);
        unlock(*first);
        return result;
    }

    [[nodiscard]] inline error_t copy(cspace_t& destination, capability_id_t destination_selector,
                                      const cspace_t& source, capability_id_t source_selector,
                                      rights_t rights_mask) noexcept {
        return derive(destination, destination_selector, source, source_selector, rights_mask, 0U);
    }

    [[nodiscard]] inline error_t mint(cspace_t& destination, capability_id_t destination_selector,
                                      const cspace_t& source, capability_id_t source_selector,
                                      rights_t rights_mask, badge_t badge) noexcept {
        return derive(destination, destination_selector, source, source_selector, rights_mask,
                      badge);
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
        const error_t result = remove_locked(cspace, selector);
        unlock(cspace);
        return result;
    }

    inline u32 revoke_descendants_locked(derivation_id_t ancestor) noexcept {
        if (ancestor == 0U)
            return 0U;

        /*
         * Revoke is deliberately two-phase.  Removing a parent capability
         * deactivates its derivation record; doing that while discovering the
         * tree would make later grandchildren appear disconnected.  Hold the
         * registered CSpaces stable, mark every descendant against the intact
         * derivation graph, and only then invalidate slots and records.
         */
        static u8 revoke_marks[maximum_registered_cspaces][cspace_slot_count]{};
        u32 removed = 0U;
        spin_lock(cspace_registry_lock);

        for (u32 space_index = 0U; space_index < maximum_registered_cspaces; ++space_index) {
            cspace_t* cspace = cspace_registry[space_index];
            if (cspace == nullptr)
                continue;
            lock(*cspace);
            for (u32 slot_index = 0U; slot_index < cspace_slot_count; ++slot_index) {
                const slot_t& slot = cspace->slots[slot_index];
                revoke_marks[space_index][slot_index] =
                    slot.object.type != object::type_t::none &&
                            descendant_of(slot.derivation, ancestor)
                        ? 1U
                        : 0U;
            }
        }

        for (u32 space_index = 0U; space_index < maximum_registered_cspaces; ++space_index) {
            cspace_t* cspace = cspace_registry[space_index];
            if (cspace == nullptr)
                continue;
            for (u32 slot_index = 0U; slot_index < cspace_slot_count; ++slot_index) {
                if (revoke_marks[space_index][slot_index] == 0U)
                    continue;
                slot_t& slot = cspace->slots[slot_index];
                if (slot.derivation < maximum_derivations)
                    __atomic_store_n(&derivations[slot.derivation].active, 0U, __ATOMIC_RELEASE);
                slot = {};
                revoke_marks[space_index][slot_index] = 0U;
                ++removed;
            }
        }

        for (u32 space_index = maximum_registered_cspaces; space_index > 0U; --space_index) {
            cspace_t* cspace = cspace_registry[space_index - 1U];
            if (cspace != nullptr)
                unlock(*cspace);
        }
        spin_unlock(cspace_registry_lock);
        return removed;
    }

    inline u32 revoke_descendants(derivation_id_t ancestor) noexcept {
        lock_authority();
        const u32 removed = revoke_descendants_locked(ancestor);
        unlock_authority();
        return removed;
    }

    inline u32 revoke_in(cspace_t& cspace, const object::reference_t& reference) noexcept {
        u32 removed = 0U;
        lock(cspace);
        for (u32 index = 0U; index < cspace_slot_count; ++index) {
            slot_t& slot = cspace.slots[index];
            if (slot.object.id == reference.id && slot.object.generation == reference.generation &&
                slot.object.type == reference.type) {
                if (slot.derivation < maximum_derivations)
                    __atomic_store_n(&derivations[slot.derivation].active, 0U, __ATOMIC_RELEASE);
                slot = {};
                ++removed;
            }
        }
        unlock(cspace);
        return removed;
    }

    inline u32 revoke_reference_locked(const object::reference_t& reference) noexcept {
        u32 removed = 0U;
        spin_lock(cspace_registry_lock);
        for (u32 space_index = 0U; space_index < maximum_registered_cspaces; ++space_index) {
            cspace_t* cspace = cspace_registry[space_index];
            if (cspace == nullptr)
                continue;
            removed += revoke_in(*cspace, reference);
        }
        spin_unlock(cspace_registry_lock);
        return removed;
    }

    inline u32 revoke_reference(const object::reference_t& reference) noexcept {
        lock_authority();
        const u32 removed = revoke_reference_locked(reference);
        unlock_authority();
        return removed;
    }
} // namespace sys::kernel::capability
