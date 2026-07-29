#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/capability.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::capability
{
    inline constexpr u32 cspace_leaf_bits = 6U;
    inline constexpr u32 cspace_leaf_slot_count = 1U << cspace_leaf_bits;
    inline constexpr u32 cspace_root_bits = 2U;
    inline constexpr u32 cspace_leaf_count = 1U << cspace_root_bits;
    inline constexpr u32 cspace_slot_count = cspace_leaf_count * cspace_leaf_slot_count;
    inline constexpr u32 cspace_guard_shift = cspace_root_bits + cspace_leaf_bits;
    inline constexpr u32 cspace_guard_mask = 0xffU;
    inline constexpr u32 maximum_registered_cspaces = 32U;
    inline constexpr u32 maximum_derivations = 4096U;
    inline constexpr u32 maximum_derivation_depth = 64U;
    inline constexpr u32 derivation_index_bits = 12U;
    inline constexpr derivation_id_t derivation_index_mask = (1ULL << derivation_index_bits) - 1U;
    static_assert(maximum_derivations == (1U << derivation_index_bits));

    struct cspace_leaf_t {
        slot_t slots[cspace_leaf_slot_count]{};
        u64 occupied{};
    };

    struct cspace_t {
        volatile u32 lock{};
        u32 registry_index{maximum_registered_cspaces};
        u32 guard{};
        u32 allocation_hint{};
        cspace_leaf_t leaves[cspace_leaf_count]{};
    };

    struct derivation_record_t {
        derivation_id_t id{};
        derivation_id_t parent{};
        object::reference_t object{};
        u32 generation{};
        volatile u32 active{};
    };

    inline cspace_t* cspace_registry[maximum_registered_cspaces]{};
    inline volatile u32 cspace_registry_lock{};
    inline derivation_record_t derivations[maximum_derivations]{};
    inline volatile u32 derivation_lock{};
    inline volatile u32 authority_lock{};
    inline derivation_id_t next_derivation_hint{1U};

    [[nodiscard]] inline constexpr u32 derivation_index(derivation_id_t id) noexcept {
        return static_cast<u32>(id & derivation_index_mask);
    }

    [[nodiscard]] inline bool has_active_derivation_child(derivation_id_t parent) noexcept {
        if (parent == 0U)
            return false;
        for (u32 index = 1U; index < maximum_derivations; ++index) {
            const derivation_record_t& candidate = derivations[index];
            if (__atomic_load_n(&candidate.active, __ATOMIC_ACQUIRE) != 0U &&
                candidate.parent == parent)
                return true;
        }
        return false;
    }

    inline void deactivate_derivation(derivation_id_t id) noexcept {
        const u32 index = derivation_index(id);
        if (id == 0U || index == 0U || index >= maximum_derivations)
            return;
        derivation_record_t& record = derivations[index];
        if (record.id == id)
            __atomic_store_n(&record.active, 0U, __ATOMIC_RELEASE);
    }

    inline void spin_lock(volatile u32& value, lock_order::rank order) noexcept {
        while (__atomic_exchange_n(&value, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&value, __ATOMIC_RELAXED) != 0U) {
                arch::cpu::relax();
            }
        }
        lock_order::acquired(order, &value);
    }

    inline void spin_unlock(volatile u32& value, lock_order::rank order) noexcept {
        lock_order::released(order, &value);
        __atomic_store_n(&value, 0U, __ATOMIC_RELEASE);
    }

    inline void lock(cspace_t& cspace) noexcept {
        spin_lock(cspace.lock, lock_order::rank::cspace);
    }
    inline void lock_authority() noexcept {
        spin_lock(authority_lock, lock_order::rank::capability_authority);
    }
    inline void unlock_authority() noexcept {
        spin_unlock(authority_lock, lock_order::rank::capability_authority);
    }
    struct authority_guard final {
        authority_guard() noexcept {
            lock_authority();
        }
        ~authority_guard() noexcept {
            if (active)
                unlock_authority();
        }
        void release() noexcept {
            if (!active)
                return;
            unlock_authority();
            active = false;
        }
        authority_guard(const authority_guard&) = delete;
        authority_guard& operator=(const authority_guard&) = delete;

      private:
        bool active{true};
    };
    inline void unlock(cspace_t& cspace) noexcept {
        spin_unlock(cspace.lock, lock_order::rank::cspace);
    }

    [[nodiscard]] inline constexpr capability_id_t encode_selector(u32 guard, u32 index) noexcept {
        return static_cast<capability_id_t>(((guard & cspace_guard_mask) << cspace_guard_shift) |
                                            index);
    }

    [[nodiscard]] inline bool resolve_selector(const cspace_t& cspace, capability_id_t selector,
                                               u32& index) noexcept {
        const u32 raw = static_cast<u32>(selector);
        index = raw & (cspace_slot_count - 1U);
        const u32 guard = (raw >> cspace_guard_shift) & cspace_guard_mask;
        return (raw >> (cspace_guard_shift + 8U)) == 0U && guard == cspace.guard;
    }

    [[nodiscard]] inline slot_t& slot_at(cspace_t& cspace, capability_id_t selector) noexcept {
        const u32 index = static_cast<u32>(selector) & (cspace_slot_count - 1U);
        return cspace.leaves[index >> cspace_leaf_bits]
            .slots[index & (cspace_leaf_slot_count - 1U)];
    }

    [[nodiscard]] inline const slot_t& slot_at(const cspace_t& cspace,
                                               capability_id_t selector) noexcept {
        const u32 index = static_cast<u32>(selector) & (cspace_slot_count - 1U);
        return cspace.leaves[index >> cspace_leaf_bits]
            .slots[index & (cspace_leaf_slot_count - 1U)];
    }

    inline void mark_occupied(cspace_t& cspace, u32 index) noexcept {
        cspace.leaves[index >> cspace_leaf_bits].occupied |=
            1ULL << (index & (cspace_leaf_slot_count - 1U));
    }

    inline void mark_free(cspace_t& cspace, u32 index) noexcept {
        cspace.leaves[index >> cspace_leaf_bits].occupied &=
            ~(1ULL << (index & (cspace_leaf_slot_count - 1U)));
    }

    [[nodiscard]] inline error_t allocate_slot(cspace_t& cspace,
                                               capability_id_t& selector) noexcept {
        lock(cspace);
        for (u32 scanned = 0U; scanned < cspace_slot_count; ++scanned) {
            const u32 index = (cspace.allocation_hint + scanned) & (cspace_slot_count - 1U);
            const u64 bit = 1ULL << (index & (cspace_leaf_slot_count - 1U));
            if ((cspace.leaves[index >> cspace_leaf_bits].occupied & bit) == 0U) {
                cspace.leaves[index >> cspace_leaf_bits].occupied |= bit;
                cspace.allocation_hint = (index + 1U) & (cspace_slot_count - 1U);
                selector = encode_selector(cspace.guard, index);
                unlock(cspace);
                return error_t::success;
            }
        }
        unlock(cspace);
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t set_guard(cspace_t& cspace, u32 guard) noexcept {
        if (guard > cspace_guard_mask)
            return error_t::invalid_argument;
        lock(cspace);
        for (u32 leaf = 0U; leaf < cspace_leaf_count; ++leaf) {
            if (cspace.leaves[leaf].occupied != 0U) {
                unlock(cspace);
                return error_t::busy;
            }
        }
        cspace.guard = guard;
        unlock(cspace);
        return error_t::success;
    }

    [[nodiscard]] inline derivation_id_t
    allocate_derivation(derivation_id_t parent, const object::reference_t& object) noexcept {
        spin_lock(derivation_lock, lock_order::rank::capability_derivation);
        derivation_id_t candidate = next_derivation_hint;
        for (u32 scanned = 1U; scanned < maximum_derivations; ++scanned) {
            if (candidate == 0U || candidate >= maximum_derivations)
                candidate = 1U;
            derivation_record_t& record = derivations[candidate];
            if (__atomic_load_n(&record.active, __ATOMIC_ACQUIRE) == 0U &&
                !has_active_derivation_child(record.id)) {
                u32 generation = record.generation + 1U;
                if (generation == 0U) {
                    ++candidate;
                    continue;
                }
                const derivation_id_t id =
                    (static_cast<derivation_id_t>(generation) << derivation_index_bits) | candidate;
                record.id = id;
                record.parent = parent;
                record.object = object;
                record.generation = generation;
                __atomic_store_n(&record.active, 1U, __ATOMIC_RELEASE);
                next_derivation_hint = candidate + 1U;
                spin_unlock(derivation_lock, lock_order::rank::capability_derivation);
                return id;
            }
            ++candidate;
        }
        spin_unlock(derivation_lock, lock_order::rank::capability_derivation);
        return 0U;
    }

    [[nodiscard]] inline bool derivation_valid(derivation_id_t id,
                                               const object::reference_t& object) noexcept {
        const u32 index = derivation_index(id);
        if (id == 0U || index == 0U || index >= maximum_derivations)
            return false;
        const derivation_record_t& record = derivations[index];
        return __atomic_load_n(&record.active, __ATOMIC_ACQUIRE) != 0U && record.id == id &&
               record.object.id == object.id && record.object.generation == object.generation &&
               record.object.type == object.type;
    }

    [[nodiscard]] inline bool descendant_of(derivation_id_t candidate,
                                            derivation_id_t ancestor) noexcept {
        if (candidate == 0U || ancestor == 0U || candidate == ancestor)
            return false;
        for (u32 depth = 0U; depth < maximum_derivation_depth; ++depth) {
            const u32 index = derivation_index(candidate);
            if (candidate == 0U || index == 0U || index >= maximum_derivations)
                return false;
            const derivation_record_t& record = derivations[index];
            if (record.id != candidate)
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
        spin_lock(cspace_registry_lock, lock_order::rank::capability_registry);
        for (u32 index = 0U; index < maximum_registered_cspaces; ++index) {
            if (cspace_registry[index] == &cspace) {
                cspace.registry_index = index;
                spin_unlock(cspace_registry_lock, lock_order::rank::capability_registry);
                return error_t::success;
            }
        }
        for (u32 index = 0U; index < maximum_registered_cspaces; ++index) {
            if (cspace_registry[index] == nullptr) {
                cspace_registry[index] = &cspace;
                cspace.registry_index = index;
                spin_unlock(cspace_registry_lock, lock_order::rank::capability_registry);
                return error_t::success;
            }
        }
        spin_unlock(cspace_registry_lock, lock_order::rank::capability_registry);
        return error_t::no_memory;
    }

    inline void initialize(cspace_t& cspace) noexcept {
        cspace.lock = 0U;
        cspace.registry_index = maximum_registered_cspaces;
        cspace.guard = 0U;
        cspace.allocation_hint = 0U;
        for (u32 leaf = 0U; leaf < cspace_leaf_count; ++leaf) {
            cspace.leaves[leaf].occupied = 0U;
            for (u32 slot = 0U; slot < cspace_leaf_slot_count; ++slot)
                cspace.leaves[leaf].slots[slot] = {};
        }
        (void)register_cspace(cspace);
    }

    [[nodiscard]] inline error_t install_locked(cspace_t& cspace, capability_id_t selector,
                                                const object::reference_t& object,
                                                rights_t granted_rights) noexcept {
        u32 index{};
        if (!resolve_selector(cspace, selector, index) || object.type == object::type_t::none ||
            object::resolve(object) == nullptr || granted_rights.bits == 0U) {
            return error_t::invalid_argument;
        }
        lock(cspace);
        slot_t& slot = slot_at(cspace, index);
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
        mark_occupied(cspace, index);
        unlock(cspace);
        return error_t::success;
    }

    [[nodiscard]] inline error_t install(cspace_t& cspace, capability_id_t selector,
                                         const object::reference_t& object,
                                         rights_t granted_rights) noexcept {
        lock_authority();
        const error_t result = install_locked(cspace, selector, object, granted_rights);
        unlock_authority();
        return result;
    }

    [[nodiscard]] inline error_t remove_locked(cspace_t& cspace,
                                               capability_id_t selector) noexcept {
        u32 index{};
        if (!resolve_selector(cspace, selector, index))
            return error_t::invalid_argument;
        slot_t& slot = slot_at(cspace, index);
        if (slot.object.type == object::type_t::none)
            return error_t::not_found;
        deactivate_derivation(slot.derivation);
        slot = {};
        mark_free(cspace, index);
        return error_t::success;
    }

    [[nodiscard]] inline error_t remove(cspace_t& cspace, capability_id_t selector) noexcept {
        lock_authority();
        lock(cspace);
        const error_t result = remove_locked(cspace, selector);
        unlock(cspace);
        unlock_authority();
        return result;
    }

    [[nodiscard]] inline error_t lookup(const cspace_t& cspace, capability_id_t selector,
                                        object::type_t expected_type, right_t required,
                                        object::header_t*& result) noexcept {
        result = nullptr;
        u32 index{};
        if (!resolve_selector(cspace, selector, index))
            return error_t::not_found;
        cspace_t& mutable_cspace = const_cast<cspace_t&>(cspace);
        lock(mutable_cspace);
        const slot_t slot = slot_at(cspace, index);
        error_t lookup_result = error_t::success;
        if (slot.object.type != expected_type)
            lookup_result = error_t::denied;
        else if (!derivation_valid(slot.derivation, slot.object))
            lookup_result = error_t::not_found;
        else
            lookup_result = validate(slot, required);
        if (lookup_result == error_t::success) {
            result = object::resolve(slot.object);
            if (result == nullptr)
                lookup_result = error_t::not_found;
        }
        unlock(mutable_cspace);
        return lookup_result;
    }

    [[nodiscard]] inline error_t lookup_slot(const cspace_t& cspace, capability_id_t selector,
                                             object::type_t expected_type, right_t required,
                                             slot_t& result) noexcept {
        result = {};
        u32 index{};
        if (!resolve_selector(cspace, selector, index))
            return error_t::not_found;
        cspace_t& mutable_cspace = const_cast<cspace_t&>(cspace);
        lock(mutable_cspace);
        const slot_t slot = slot_at(cspace, index);
        error_t lookup_result = error_t::success;
        if (slot.object.type != expected_type)
            lookup_result = error_t::denied;
        else if (!derivation_valid(slot.derivation, slot.object))
            lookup_result = error_t::not_found;
        else
            lookup_result = validate(slot, required);
        if (lookup_result == error_t::success && object::resolve(slot.object) == nullptr)
            lookup_result = error_t::not_found;
        if (lookup_result == error_t::success)
            result = slot;
        unlock(mutable_cspace);
        return lookup_result;
    }
    [[nodiscard]] inline error_t derive_locked(cspace_t& destination,
                                               capability_id_t destination_selector,
                                               const cspace_t& source,
                                               capability_id_t source_selector,
                                               rights_t rights_mask, badge_t badge) noexcept {
        u32 destination_index{};
        u32 source_index{};
        if (!resolve_selector(destination, destination_selector, destination_index) ||
            !resolve_selector(source, source_selector, source_index))
            return error_t::invalid_argument;
        cspace_t& mutable_source = const_cast<cspace_t&>(source);
        cspace_t* first = &destination < &mutable_source ? &destination : &mutable_source;
        cspace_t* second = &destination < &mutable_source ? &mutable_source : &destination;
        lock(*first);
        if (second != first)
            lock(*second);
        const slot_t source_slot = slot_at(source, source_index);
        slot_t& destination_slot = slot_at(destination, destination_index);
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
                mark_occupied(destination, destination_index);
            }
        }
        if (second != first)
            unlock(*second);
        unlock(*first);
        return result;
    }

    [[nodiscard]] inline error_t derive(cspace_t& destination, capability_id_t destination_selector,
                                        const cspace_t& source, capability_id_t source_selector,
                                        rights_t rights_mask, badge_t badge) noexcept {
        lock_authority();
        const error_t result = derive_locked(destination, destination_selector, source,
                                             source_selector, rights_mask, badge);
        unlock_authority();
        return result;
    }

    [[nodiscard]] inline error_t
    copy_locked(cspace_t& destination, capability_id_t destination_selector, const cspace_t& source,
                capability_id_t source_selector, rights_t rights_mask) noexcept {
        return derive_locked(destination, destination_selector, source, source_selector,
                             rights_mask, 0U);
    }

    [[nodiscard]] inline error_t copy(cspace_t& destination, capability_id_t destination_selector,
                                      const cspace_t& source, capability_id_t source_selector,
                                      rights_t rights_mask) noexcept {
        return derive(destination, destination_selector, source, source_selector, rights_mask, 0U);
    }

    [[nodiscard]] inline error_t
    mint_locked(cspace_t& destination, capability_id_t destination_selector, const cspace_t& source,
                capability_id_t source_selector, rights_t rights_mask, badge_t badge) noexcept {
        return derive_locked(destination, destination_selector, source, source_selector,
                             rights_mask, badge);
    }

    [[nodiscard]] inline error_t mint(cspace_t& destination, capability_id_t destination_selector,
                                      const cspace_t& source, capability_id_t source_selector,
                                      rights_t rights_mask, badge_t badge) noexcept {
        return derive(destination, destination_selector, source, source_selector, rights_mask,
                      badge);
    }

    [[nodiscard]] inline error_t move_locked(cspace_t& destination,
                                             capability_id_t destination_selector, cspace_t& source,
                                             capability_id_t source_selector) noexcept {
        if (&destination == &source && destination_selector == source_selector)
            return error_t::success;
        u32 destination_index{};
        u32 source_index{};
        if (!resolve_selector(destination, destination_selector, destination_index) ||
            !resolve_selector(source, source_selector, source_index))
            return error_t::invalid_argument;
        cspace_t* first = &destination < &source ? &destination : &source;
        cspace_t* second = &destination < &source ? &source : &destination;
        lock(*first);
        if (second != first)
            lock(*second);
        slot_t& source_slot = slot_at(source, source_index);
        slot_t& destination_slot = slot_at(destination, destination_index);
        error_t result = error_t::success;
        if (source_slot.object.type == object::type_t::none)
            result = error_t::not_found;
        else if (destination_slot.object.type != object::type_t::none)
            result = error_t::busy;
        else {
            destination_slot = source_slot;
            source_slot = {};
            mark_occupied(destination, destination_index);
            mark_free(source, source_index);
        }
        if (second != first)
            unlock(*second);
        unlock(*first);
        return result;
    }

    [[nodiscard]] inline error_t move(cspace_t& destination, capability_id_t destination_selector,
                                      cspace_t& source, capability_id_t source_selector) noexcept {
        lock_authority();
        const error_t result =
            move_locked(destination, destination_selector, source, source_selector);
        unlock_authority();
        return result;
    }

    [[nodiscard]] inline error_t delete_capability_locked(cspace_t& cspace,
                                                          capability_id_t selector) noexcept {
        lock(cspace);
        const error_t result = remove_locked(cspace, selector);
        unlock(cspace);
        return result;
    }

    [[nodiscard]] inline error_t delete_capability(cspace_t& cspace,
                                                   capability_id_t selector) noexcept {
        lock_authority();
        const error_t result = delete_capability_locked(cspace, selector);
        unlock_authority();
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
        cspace_t* locked_spaces[maximum_registered_cspaces];
        u32 locked_count = 0U;
        u32 removed = 0U;
        spin_lock(cspace_registry_lock, lock_order::rank::capability_registry);

        for (u32 space_index = 0U; space_index < maximum_registered_cspaces; ++space_index) {
            cspace_t* cspace = cspace_registry[space_index];
            if (cspace == nullptr)
                continue;
            u32 position = locked_count;
            while (position > 0U && reinterpret_cast<uintptr_t>(locked_spaces[position - 1U]) >
                                        reinterpret_cast<uintptr_t>(cspace)) {
                locked_spaces[position] = locked_spaces[position - 1U];
                --position;
            }
            locked_spaces[position] = cspace;
            ++locked_count;
        }
        for (u32 index = 0U; index < locked_count; ++index)
            lock(*locked_spaces[index]);

        for (u32 space_index = 0U; space_index < maximum_registered_cspaces; ++space_index) {
            cspace_t* cspace = cspace_registry[space_index];
            if (cspace == nullptr)
                continue;
            for (u32 slot_index = 0U; slot_index < cspace_slot_count; ++slot_index) {
                const slot_t& slot = slot_at(*cspace, slot_index);
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
                slot_t& slot = slot_at(*cspace, slot_index);
                deactivate_derivation(slot.derivation);
                slot = {};
                mark_free(*cspace, slot_index);
                revoke_marks[space_index][slot_index] = 0U;
                ++removed;
            }
        }

        for (u32 index = locked_count; index > 0U; --index)
            unlock(*locked_spaces[index - 1U]);
        spin_unlock(cspace_registry_lock, lock_order::rank::capability_registry);
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
            slot_t& slot = slot_at(cspace, index);
            if (slot.object.id == reference.id && slot.object.generation == reference.generation &&
                slot.object.type == reference.type) {
                deactivate_derivation(slot.derivation);
                slot = {};
                mark_free(cspace, index);
                ++removed;
            }
        }
        unlock(cspace);
        return removed;
    }

    inline u32 revoke_reference_locked(const object::reference_t& reference) noexcept {
        u32 removed = 0U;
        spin_lock(cspace_registry_lock, lock_order::rank::capability_registry);
        for (u32 space_index = 0U; space_index < maximum_registered_cspaces; ++space_index) {
            cspace_t* cspace = cspace_registry[space_index];
            if (cspace == nullptr)
                continue;
            removed += revoke_in(*cspace, reference);
        }
        spin_unlock(cspace_registry_lock, lock_order::rank::capability_registry);
        return removed;
    }

    inline u32 revoke_reference(const object::reference_t& reference) noexcept {
        lock_authority();
        const u32 removed = revoke_reference_locked(reference);
        unlock_authority();
        return removed;
    }

    template <typename RetireAttachment>
    inline u32 revoke_all_locked(cspace_t& cspace, RetireAttachment&& retire_attachment) noexcept {
        u32 removed = 0U;
        for (u32 index = 0U; index < cspace_slot_count; ++index) {
            lock(cspace);
            const slot_t slot = slot_at(cspace, index);
            unlock(cspace);
            if (slot.object.type == object::type_t::none)
                continue;
            retire_attachment(slot.derivation);
            removed += revoke_descendants_locked(slot.derivation);
            if (delete_capability_locked(cspace, encode_selector(cspace.guard, index)) ==
                error_t::success)
                ++removed;
        }
        return removed;
    }
} // namespace sys::kernel::capability
