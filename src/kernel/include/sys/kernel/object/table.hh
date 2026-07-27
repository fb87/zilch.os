#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::object
{
    inline constexpr u32 table_capacity = 512U;
    inline constexpr object_id_t dynamic_id_base = 96U;
    inline constexpr u32 maximum_reader_cpus = 64U;

    namespace bootstrap_id
    {
        inline constexpr object_id_t thread_base = 0U;
        inline constexpr object_id_t task_base = 16U;
        inline constexpr object_id_t endpoint_base = 32U;
        inline constexpr object_id_t frame_base = 40U;
        inline constexpr object_id_t page_table_base = 44U;
        inline constexpr object_id_t notification = 48U;
        inline constexpr object_id_t timer_interrupt = 49U;
        inline constexpr object_id_t scheduling_context_base = 50U;
        inline constexpr object_id_t address_space_base = 60U;
        inline constexpr object_id_t virtual_machine = 80U;
        inline constexpr object_id_t virtual_cpu = 81U;
        inline constexpr object_id_t root_memory_resource = 82U;

        static_assert(thread_base + 10U <= task_base);
        static_assert(task_base + 10U <= endpoint_base);
        static_assert(endpoint_base + 2U <= frame_base);
        static_assert(frame_base + 4U <= page_table_base);
        static_assert(page_table_base + 4U <= notification);
        static_assert(timer_interrupt < scheduling_context_base);
        static_assert(scheduling_context_base + 10U <= address_space_base);
        static_assert(address_space_base + 10U <= virtual_machine);
        static_assert(virtual_cpu < root_memory_resource);
        static_assert(root_memory_resource < dynamic_id_base);
    } // namespace bootstrap_id

    struct reference_t {
        object_id_t id{};
        u32 generation{};
        type_t type{type_t::none};
        u8 reserved[3]{};
    };

    struct table_slot_t {
        header_t* object{};
        u32 generation{};
    };

    inline table_slot_t object_table[table_capacity]{};
    inline volatile u32 table_lock{};
    inline u32 reader_depth[maximum_reader_cpus]{};

    inline void lock_table() noexcept {
        while (__atomic_exchange_n(&table_lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            while (__atomic_load_n(&table_lock, __ATOMIC_RELAXED) != 0U)
                arch::cpu::relax();
    }

    inline void unlock_table() noexcept {
        __atomic_store_n(&table_lock, 0U, __ATOMIC_RELEASE);
    }

    inline void enter_read_side() noexcept {
        const u32 cpu = arch::cpu::current_id();
        if (cpu < maximum_reader_cpus)
            __atomic_add_fetch(&reader_depth[cpu], 1U, __ATOMIC_SEQ_CST);
    }

    inline void leave_read_side() noexcept {
        const u32 cpu = arch::cpu::current_id();
        if (cpu < maximum_reader_cpus)
            __atomic_sub_fetch(&reader_depth[cpu], 1U, __ATOMIC_SEQ_CST);
    }

    struct read_guard final {
        read_guard() noexcept {
            enter_read_side();
        }
        ~read_guard() noexcept {
            leave_read_side();
        }
        read_guard(const read_guard&) = delete;
        read_guard& operator=(const read_guard&) = delete;
    };

    inline void synchronize_readers() noexcept {
        const u32 current_cpu = arch::cpu::current_id();
        for (u32 cpu = 0U; cpu < maximum_reader_cpus; ++cpu) {
            if (cpu == current_cpu)
                continue;
            while (__atomic_load_n(&reader_depth[cpu], __ATOMIC_SEQ_CST) != 0U)
                arch::cpu::relax();
        }
    }

    [[nodiscard]] inline constexpr reference_t null_reference() noexcept {
        return {};
    }

    [[nodiscard]] inline reference_t reference(const header_t& object) noexcept {
        return {object.id, object.generation, object.type, {0U, 0U, 0U}};
    }

    [[nodiscard]] inline error_t register_object(header_t& object, object_id_t id,
                                                 type_t type) noexcept {
        if (id >= table_capacity || type == type_t::none)
            return error_t::invalid_argument;
        lock_table();
        table_slot_t& slot = object_table[id];
        if (slot.object != nullptr) {
            unlock_table();
            return error_t::busy;
        }
        u32 generation = slot.generation + 1U;
        if (generation == 0U)
            generation = 1U;
        object.id = id;
        object.type = type;
        object.generation = generation;
        object.flags = 0U;
        slot.generation = generation;
        __atomic_store_n(&slot.object, &object, __ATOMIC_RELEASE);
        unlock_table();
        return error_t::success;
    }

    [[nodiscard]] inline error_t register_dynamic_object(header_t& object, type_t type) noexcept {
        if (type == type_t::none)
            return error_t::invalid_argument;
        lock_table();
        for (object_id_t id = dynamic_id_base; id < table_capacity; ++id) {
            table_slot_t& slot = object_table[id];
            if (slot.object != nullptr)
                continue;
            u32 generation = slot.generation + 1U;
            if (generation == 0U)
                generation = 1U;
            object.id = id;
            object.type = type;
            object.generation = generation;
            object.flags = 0U;
            slot.generation = generation;
            __atomic_store_n(&slot.object, &object, __ATOMIC_RELEASE);
            unlock_table();
            return error_t::success;
        }
        unlock_table();
        return error_t::no_memory;
    }

    [[nodiscard]] inline header_t* resolve(const reference_t& reference) noexcept {
        if (reference.id >= table_capacity || reference.type == type_t::none)
            return nullptr;
        const table_slot_t& slot = object_table[reference.id];
        header_t* object = __atomic_load_n(&slot.object, __ATOMIC_ACQUIRE);
        if (object == nullptr || slot.generation != reference.generation ||
            object->generation != reference.generation || object->type != reference.type)
            return nullptr;
        return object;
    }

    [[nodiscard]] inline error_t unregister_object(const reference_t& reference) noexcept {
        if (reference.id >= table_capacity)
            return error_t::invalid_argument;
        table_slot_t& slot = object_table[reference.id];
        header_t* expected = resolve(reference);
        if (expected == nullptr)
            return error_t::not_found;
        if (!__atomic_compare_exchange_n(&slot.object, &expected, nullptr, false, __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE))
            return error_t::busy;
        /*
         * The table removal prevents new generation-checked resolutions.
         * Readers that resolved the old pointer before removal must leave
         * their exception dispatch before the backing object can be reused.
         * The current CPU is excluded because destruction itself runs inside
         * a read-side section and already owns all of its local references.
         */
        synchronize_readers();
        return error_t::success;
    }

    static_assert(sizeof(reference_t) == 16U);
} // namespace sys::kernel::object
