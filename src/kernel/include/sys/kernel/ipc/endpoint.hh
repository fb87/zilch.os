#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/platform/interrupt.hh>
#include <sys/types.hh>

namespace sys::kernel::ipc
{
    inline constexpr u32 endpoint_capacity = 16U;

    struct endpoint
    {
        volatile u32 lock{};
        thread_id_t senders[endpoint_capacity]{};
        u32 sender_head{};
        u32 sender_tail{};
        u32 sender_count{};
        thread_id_t receiver{static_cast<thread_id_t>(-1)};
    };

    inline endpoint endpoints[2]{};

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

    [[nodiscard]] inline endpoint* lookup(capability_id_t selector) noexcept
    {
        if (selector == 10U) return &endpoints[0];
        if (selector == 11U) return &endpoints[1];
        return nullptr;
    }

    [[nodiscard]] inline bool enqueue_sender(endpoint& value,
                                             thread_id_t id) noexcept
    {
        if (value.sender_count == endpoint_capacity) return false;
        value.senders[value.sender_tail] = id;
        value.sender_tail = (value.sender_tail + 1U) % endpoint_capacity;
        ++value.sender_count;
        return true;
    }

    [[nodiscard]] inline bool dequeue_sender(endpoint& value,
                                             thread_id_t& id) noexcept
    {
        if (value.sender_count == 0U) return false;
        id = value.senders[value.sender_head];
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

    inline void remote_reschedule(cpu_id_t target,
                                  cpu_id_t current) noexcept
    {
        if (target != current) {
            platform::interrupt::send_ipi_all_others(
                platform::interrupt::reschedule_ipi);
        }
    }
} // namespace sys::kernel::ipc
