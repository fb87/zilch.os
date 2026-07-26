#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/platform/interrupt.hh>
#include <sys/types.hh>

namespace sys::kernel::ipc
{
    inline constexpr u32 endpoint_capacity = 16U;
    inline constexpr u32 endpoint_count = 2U;

    struct endpoint
    {
        object::header_t object{};
        volatile u32 lock{};
        object::reference_t senders[endpoint_capacity]{};
        u32 sender_head{};
        u32 sender_tail{};
        u32 sender_count{};
        object::reference_t receiver{};
    };

    inline endpoint endpoints[endpoint_count]{};

    inline void initialize(endpoint& value) noexcept
    {
        value.lock = 0U;
        value.sender_head = 0U;
        value.sender_tail = 0U;
        value.sender_count = 0U;
        value.receiver = {};
    }

    inline void lock(endpoint& value) noexcept
    {
        while (__atomic_exchange_n(&value.lock, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&value.lock, __ATOMIC_RELAXED) != 0U) {
                arch::cpu::relax();
            }
        }
    }

    inline void unlock(endpoint& value) noexcept
    {
        __atomic_store_n(&value.lock, 0U, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool enqueue_sender(endpoint& value,
                                             const object::reference_t& reference) noexcept
    {
        if (value.sender_count == endpoint_capacity) return false;
        value.senders[value.sender_tail] = reference;
        value.sender_tail = (value.sender_tail + 1U) % endpoint_capacity;
        ++value.sender_count;
        return true;
    }

    [[nodiscard]] inline bool dequeue_sender(endpoint& value,
                                             object::reference_t& reference) noexcept
    {
        if (value.sender_count == 0U) return false;
        reference = value.senders[value.sender_head];
        value.senders[value.sender_head] = {};
        value.sender_head = (value.sender_head + 1U) % endpoint_capacity;
        --value.sender_count;
        return true;
    }

    [[nodiscard]] inline bool validate(const endpoint& value) noexcept
    {
        return value.sender_count <= endpoint_capacity
            && value.sender_head < endpoint_capacity
            && value.sender_tail < endpoint_capacity;
    }


    inline u32 cancel_thread(endpoint& value,
                             const object::reference_t& thread_reference) noexcept
    {
        u32 cancelled = 0U;
        lock(value);
        if (value.receiver.id == thread_reference.id
            && value.receiver.generation == thread_reference.generation
            && value.receiver.type == thread_reference.type) {
            value.receiver = {};
            ++cancelled;
        }
        object::reference_t retained[endpoint_capacity]{};
        u32 retained_count = 0U;
        while (value.sender_count != 0U) {
            object::reference_t candidate{};
            (void)dequeue_sender(value, candidate);
            if (candidate.id == thread_reference.id
                && candidate.generation == thread_reference.generation
                && candidate.type == thread_reference.type) {
                ++cancelled;
            } else {
                retained[retained_count++] = candidate;
            }
        }
        for (u32 index = 0U; index < retained_count; ++index) {
            (void)enqueue_sender(value, retained[index]);
        }
        unlock(value);
        return cancelled;
    }

    template <typename Cancel>
    inline void destroy(endpoint& value, Cancel&& cancel) noexcept
    {
        lock(value);
        if (value.receiver.type != object::type_t::none) cancel(value.receiver);
        value.receiver = {};
        while (value.sender_count != 0U) {
            object::reference_t sender{};
            (void)dequeue_sender(value, sender);
            cancel(sender);
        }
        unlock(value);
    }

    inline void remote_reschedule(cpu_id_t target,
                                  cpu_id_t current) noexcept
    {
        if (target != current) {
            platform::interrupt::send_ipi_all_others(
                platform::interrupt::reschedule_ipi);
        }
    }
} // namespace sys::kernel::ipc
