#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/interrupt/timing.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/platform/interrupt.hh>
#include <sys/types.hh>

namespace sys::kernel::ipc
{
    inline constexpr u32 endpoint_capacity = 16U;
    inline constexpr u32 endpoint_count = 2U;
    inline constexpr u32 dynamic_endpoint_count = 16U;

    struct endpoint {
        object::header_t object{};
        volatile u32 lock{};
        object::reference_t senders[endpoint_capacity]{};
        u32 sender_head{};
        u32 sender_tail{};
        u32 sender_count{};
        object::reference_t receiver{};
        volatile u32 allocated{};
        bool retiring{};
    };

    inline endpoint endpoints[endpoint_count]{};
    inline endpoint dynamic_endpoints[dynamic_endpoint_count]{};

    inline void initialize(endpoint& value) noexcept {
        value.lock = 0U;
        value.sender_head = 0U;
        value.sender_tail = 0U;
        value.sender_count = 0U;
        value.receiver = {};
        value.retiring = false;
    }

    inline void lock(endpoint& value) noexcept {
        while (__atomic_exchange_n(&value.lock, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&value.lock, __ATOMIC_RELAXED) != 0U) {
                arch::cpu::relax();
            }
        }
        lock_order::acquired(lock_order::rank::endpoint, &value.lock);
    }

    inline void unlock(endpoint& value) noexcept {
        lock_order::released(lock_order::rank::endpoint, &value.lock);
        __atomic_store_n(&value.lock, 0U, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool enqueue_sender(endpoint& value,
                                             const object::reference_t& reference) noexcept {
        if (value.sender_count == endpoint_capacity)
            return false;
        value.senders[value.sender_tail] = reference;
        value.sender_tail = (value.sender_tail + 1U) % endpoint_capacity;
        ++value.sender_count;
        return true;
    }

    [[nodiscard]] inline bool dequeue_sender(endpoint& value,
                                             object::reference_t& reference) noexcept {
        if (value.sender_count == 0U)
            return false;
        reference = value.senders[value.sender_head];
        value.senders[value.sender_head] = {};
        value.sender_head = (value.sender_head + 1U) % endpoint_capacity;
        --value.sender_count;
        return true;
    }

    [[nodiscard]] inline bool validate(const endpoint& value) noexcept {
        if (value.sender_count > endpoint_capacity || value.sender_head >= endpoint_capacity ||
            value.sender_tail >= endpoint_capacity ||
            (value.retiring &&
             (value.sender_count != 0U || value.receiver.type != object::type_t::none)))
            return false;
        for (u32 offset = 0U; offset < value.sender_count; ++offset) {
            const u32 index = (value.sender_head + offset) % endpoint_capacity;
            const object::reference_t& sender = value.senders[index];
            if (sender.type != object::type_t::thread || object::resolve(sender) == nullptr ||
                (value.receiver.type == object::type_t::thread && sender.id == value.receiver.id &&
                 sender.generation == value.receiver.generation))
                return false;
            for (u32 previous = 0U; previous < offset; ++previous) {
                const object::reference_t& candidate =
                    value.senders[(value.sender_head + previous) % endpoint_capacity];
                if (candidate.id == sender.id && candidate.generation == sender.generation)
                    return false;
            }
        }
        return value.receiver.type == object::type_t::none ||
               (value.receiver.type == object::type_t::thread &&
                object::resolve(value.receiver) != nullptr);
    }

    inline u32 cancel_thread_locked(endpoint& value,
                                    const object::reference_t& thread_reference) noexcept {
        u32 cancelled = 0U;
        if (value.receiver.id == thread_reference.id &&
            value.receiver.generation == thread_reference.generation &&
            value.receiver.type == thread_reference.type) {
            value.receiver = {};
            ++cancelled;
        }
        object::reference_t retained[endpoint_capacity]{};
        u32 retained_count = 0U;
        while (value.sender_count != 0U) {
            object::reference_t candidate{};
            (void)dequeue_sender(value, candidate);
            if (candidate.id == thread_reference.id &&
                candidate.generation == thread_reference.generation &&
                candidate.type == thread_reference.type) {
                ++cancelled;
            } else {
                retained[retained_count++] = candidate;
            }
        }
        for (u32 index = 0U; index < retained_count; ++index) {
            (void)enqueue_sender(value, retained[index]);
        }
        return cancelled;
    }

    inline u32 cancel_thread(endpoint& value,
                             const object::reference_t& thread_reference) noexcept {
        lock(value);
        const u32 cancelled = cancel_thread_locked(value, thread_reference);
        unlock(value);
        return cancelled;
    }

    template <typename Cancel> inline void destroy(endpoint& value, Cancel&& cancel) noexcept {
        lock(value);
        if (value.receiver.type != object::type_t::none)
            cancel(value.receiver);
        value.receiver = {};
        while (value.sender_count != 0U) {
            object::reference_t sender{};
            (void)dequeue_sender(value, sender);
            cancel(sender);
        }
        unlock(value);
    }

    [[nodiscard]] inline error_t create(task::task& owner, capability_id_t selector) noexcept {
        if (selector >= capability::cspace_slot_count)
            return error_t::invalid_argument;
        if (capability::slot_at(owner.cspace, selector).object.type != object::type_t::none)
            return error_t::busy;
        for (u32 index = 0U; index < dynamic_endpoint_count; ++index) {
            endpoint& value = dynamic_endpoints[index];
            u32 expected = 0U;
            if (!__atomic_compare_exchange_n(&value.allocated, &expected, 1U, false,
                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                continue;
            initialize(value);
            error_t result =
                object::register_dynamic_object(value.object, object::type_t::endpoint);
            if (result == error_t::success) {
                result =
                    capability::install(owner.cspace, selector, object::reference(value.object),
                                        {static_cast<u32>(capability::right_t::read) |
                                         static_cast<u32>(capability::right_t::write) |
                                         static_cast<u32>(capability::right_t::grant) |
                                         static_cast<u32>(capability::right_t::control)});
            }
            if (result != error_t::success) {
                if (value.object.type != object::type_t::none)
                    (void)object::unregister_object(object::reference(value.object));
                value.object = {};
                __atomic_store_n(&value.allocated, 0U, __ATOMIC_RELEASE);
            }
            return result;
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t destroy(task::task& owner, capability_id_t selector) noexcept {
        object::header_t* header = nullptr;
        error_t result = capability::lookup(owner.cspace, selector, object::type_t::endpoint,
                                            capability::right_t::control, header);
        if (result != error_t::success)
            return result;
        auto& value = *reinterpret_cast<endpoint*>(header);
        if (&value < dynamic_endpoints || &value >= dynamic_endpoints + dynamic_endpoint_count)
            return error_t::denied;
        lock(value);
        capability::authority_guard authority_transaction{};
        header = nullptr;
        result = capability::lookup(owner.cspace, selector, object::type_t::endpoint,
                                    capability::right_t::control, header);
        if (result != error_t::success || header != &value.object) {
            authority_transaction.release();
            unlock(value);
            return result == error_t::success ? error_t::not_found : result;
        }
        const bool busy = value.receiver.type != object::type_t::none || value.sender_count != 0U;
        if (busy || value.retiring) {
            authority_transaction.release();
            unlock(value);
            return error_t::busy;
        }
        value.retiring = true;
        const object::reference_t reference = object::reference(value.object);
        capability::revoke_reference_locked(reference);
        authority_transaction.release();
        unlock(value);
        result = object::unregister_object(reference);
        if (result != error_t::success)
            return result;
        value.object = {};
        initialize(value);
        __atomic_store_n(&value.allocated, 0U, __ATOMIC_RELEASE);
        return error_t::success;
    }

    [[nodiscard]] inline bool database_valid() noexcept {
        for (endpoint& value : endpoints) {
            lock(value);
            const bool valid = validate(value);
            unlock(value);
            if (!valid)
                return false;
        }
        for (endpoint& value : dynamic_endpoints) {
            lock(value);
            const bool allocated = __atomic_load_n(&value.allocated, __ATOMIC_ACQUIRE) != 0U;
            const bool valid =
                validate(value) && (allocated ? value.object.type == object::type_t::endpoint
                                              : value.object.type == object::type_t::none &&
                                                    !value.retiring && value.sender_count == 0U &&
                                                    value.receiver.type == object::type_t::none);
            unlock(value);
            if (!valid)
                return false;
        }
        return true;
    }

    inline void remote_reschedule(cpu_id_t target, cpu_id_t current) noexcept {
        if (target != current) {
            interrupt::timing::begin_cross_cpu_wake(target);
            platform::interrupt::send_ipi(target, platform::interrupt::reschedule_ipi);
        }
    }
} // namespace sys::kernel::ipc
