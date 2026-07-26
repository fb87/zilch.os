#pragma once

#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::object
{
    inline constexpr u32 table_capacity = 96U;

    struct reference_t
    {
        object_id_t id{};
        u32 generation{};
        type_t type{type_t::none};
        u8 reserved[3]{};
    };

    struct table_slot_t
    {
        header_t* object{};
        u32 generation{};
    };

    inline table_slot_t object_table[table_capacity]{};

    [[nodiscard]] inline constexpr reference_t null_reference() noexcept
    {
        return {};
    }

    [[nodiscard]] inline reference_t reference(const header_t& object) noexcept
    {
        return {object.id, object.generation, object.type, {0U, 0U, 0U}};
    }

    [[nodiscard]] inline error_t register_object(header_t& object,
                                                 object_id_t id,
                                                 type_t type) noexcept
    {
        if (id >= table_capacity || type == type_t::none) {
            return error_t::invalid_argument;
        }
        table_slot_t& slot = object_table[id];
        if (slot.object != nullptr) return error_t::busy;

        u32 generation = slot.generation + 1U;
        if (generation == 0U) generation = 1U;
        object.id = id;
        object.type = type;
        object.generation = generation;
        object.flags = 0U;
        slot.generation = generation;
        __atomic_store_n(&slot.object, &object, __ATOMIC_RELEASE);
        return error_t::success;
    }

    [[nodiscard]] inline header_t* resolve(const reference_t& reference) noexcept
    {
        if (reference.id >= table_capacity || reference.type == type_t::none) {
            return nullptr;
        }
        const table_slot_t& slot = object_table[reference.id];
        header_t* object = __atomic_load_n(&slot.object, __ATOMIC_ACQUIRE);
        if (object == nullptr || slot.generation != reference.generation
            || object->generation != reference.generation
            || object->type != reference.type) {
            return nullptr;
        }
        return object;
    }

    [[nodiscard]] inline error_t unregister_object(
        const reference_t& reference) noexcept
    {
        if (reference.id >= table_capacity) return error_t::invalid_argument;
        table_slot_t& slot = object_table[reference.id];
        header_t* expected = resolve(reference);
        if (expected == nullptr) return error_t::not_found;
        if (!__atomic_compare_exchange_n(&slot.object, &expected, nullptr,
                                         false, __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE)) {
            return error_t::busy;
        }
        return error_t::success;
    }

    static_assert(sizeof(reference_t) == 16U);
} // namespace sys::kernel::object
