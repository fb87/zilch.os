#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/context.hh>
#include <sys/kernel/fault/fault.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/scheduling/context.hh>
#include <sys/kernel/space/address_space.hh>
#include <sys/kernel/task/task.hh>
#include <sys/types.hh>

#include <abi/sys/v1/syscall_numbers.hh>

namespace sys::kernel::thread
{
    /*
     * Serializes ownership changes for blocked IPC state, pending results,
     * and single-use reply authority.  Endpoint queues are always locked
     * before this lock; code that starts here must not acquire an endpoint
     * lock until it releases the lifecycle lock.
     */
    inline volatile u32 ipc_lifecycle_lock{};

    inline void lock_ipc_lifecycle() noexcept {
        while (__atomic_exchange_n(&ipc_lifecycle_lock, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&ipc_lifecycle_lock, __ATOMIC_RELAXED) != 0U)
                arch::cpu::relax();
        }
        lock_order::acquired(lock_order::rank::ipc_lifecycle, &ipc_lifecycle_lock);
    }

    [[nodiscard]] inline bool try_lock_ipc_lifecycle() noexcept {
        u32 expected = 0U;
        const bool acquired = __atomic_compare_exchange_n(&ipc_lifecycle_lock, &expected, 1U, false,
                                                          __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
        if (acquired)
            lock_order::acquired(lock_order::rank::ipc_lifecycle, &ipc_lifecycle_lock);
        return acquired;
    }

    inline void unlock_ipc_lifecycle() noexcept {
        lock_order::released(lock_order::rank::ipc_lifecycle, &ipc_lifecycle_lock);
        __atomic_store_n(&ipc_lifecycle_lock, 0U, __ATOMIC_RELEASE);
    }

    enum class state : u8 {
        inactive,
        ready,
        running,
        blocked_send,
        blocked_receive,
        blocked_reply,
        blocked_fault,
        suspended,
        faulted,
        terminated,
    };

    enum class pending_ipc : u8 {
        none,
        incoming_call,
        reply,
    };

    struct reply_capability {
        thread_id_t caller{static_cast<thread_id_t>(-1)};
        u32 generation{};
        u32 nonce{};
        bool donation_active{};
        bool valid{};
    };

    struct capability_transfer {
        capability_id_t source{static_cast<capability_id_t>(-1)};
        capability_id_t destination{static_cast<capability_id_t>(-1)};
        capability::rights_t rights{};
        capability::badge_t badge{};
        bool valid{};
    };

    inline constexpr u32 maximum_capability_transfers =
        static_cast<u32>(abi::v1::maximum_capability_transfers);

    struct capability_transfer_set {
        capability_transfer entries[maximum_capability_transfers]{};
        u32 count{};

        [[nodiscard]] bool valid() const noexcept {
            return count != 0U && count <= maximum_capability_transfers;
        }
    };

    inline void clear_transfer(capability_transfer_set& value) noexcept {
        for (u32 index = 0U; index < maximum_capability_transfers; ++index) {
            value.entries[index].source = static_cast<capability_id_t>(-1);
            value.entries[index].destination = static_cast<capability_id_t>(-1);
            value.entries[index].rights = {};
            value.entries[index].badge = 0U;
            value.entries[index].valid = false;
        }
        value.count = 0U;
    }

    struct thread {
        object::header_t object{};
        thread_id_t id{};
        cpu_id_t pinned_cpu{};
        state current_state{state::inactive};
        space::address_space address_space{};
        arch::thread::context context{};
        task::task* owner{};
        scheduling::context scheduling_context{};
        capability_id_t waiting_endpoint{};
        reply_capability reply{};
        capability_transfer_set transfer{};
        u64 ipc_deadline{};
        bool ipc_timeout_active{};
        error_t pending_result{error_t::success};
        word_t message[4]{};
        word_t pending_message[4]{};
        thread_id_t pending_sender{static_cast<thread_id_t>(-1)};
        u32 pending_sender_generation{};
        capability::badge_t pending_badge{};
        capability::badge_t ipc_badge{};
        volatile u8 pending_ipc_kind{static_cast<u8>(pending_ipc::none)};
        u64 reports{};
        u64 fuzz_seed{};
        u64 fuzz_iterations{};
        u64 fuzz_failures{};
        u64 faults{};
        fault::record last_fault{};
        fault::disposition fault_disposition{fault::disposition::pending};
        volatile bool executing{};
    };

    [[nodiscard]] inline constexpr u64 initial_fuzz_seed(thread_id_t id) noexcept {
        return 0x9e3779b97f4a7c15ULL ^ (static_cast<u64>(id) * 0xbf58476d1ce4e5b9ULL);
    }

    [[nodiscard]] inline error_t initialize_user(thread& value, thread_id_t id, cpu_id_t cpu,
                                                 word_t argument0, word_t argument1) noexcept {
        value.id = id;
        value.pinned_cpu = cpu;
        value.current_state = state::inactive;
        value.waiting_endpoint = 0U;
        scheduling::initialize(value.scheduling_context, cpu);
        value.reply = {};
        clear_transfer(value.transfer);
        value.ipc_deadline = 0U;
        value.ipc_timeout_active = false;
        value.pending_result = error_t::success;
        value.pending_sender = static_cast<thread_id_t>(-1);
        value.pending_sender_generation = 0U;
        value.pending_badge = 0U;
        value.ipc_badge = 0U;
        value.pending_ipc_kind = static_cast<u8>(pending_ipc::none);
        for (usize_t index = 0U; index < 4U; ++index) {
            value.pending_message[index] = 0U;
        }
        value.reports = 0U;
        value.fuzz_seed = static_cast<u64>(argument1);
        value.fuzz_iterations = 0U;
        value.fuzz_failures = 0U;
        value.faults = 0U;
        value.last_fault = {};
        value.fault_disposition = fault::disposition::pending;
        __atomic_store_n(&value.executing, false, __ATOMIC_RELAXED);
        const error_t space_result = value.address_space.initialize(
            static_cast<space_id_t>(id), argument0, &memory::allocate_physical_page,
            &memory::release_physical_page);
        if (space_result != error_t::success)
            return space_result;
        arch::thread::initialize_user(value.context, arch::space::entry(value.address_space.native),
                                      arch::space::stack_top(), argument0, argument1);
        return error_t::success;
    }

    [[nodiscard]] inline state load_state(const thread& value) noexcept {
        return static_cast<state>(
            __atomic_load_n(reinterpret_cast<const u8*>(&value.current_state), __ATOMIC_ACQUIRE));
    }

    inline void store_state(thread& value, state new_state) noexcept {
        __atomic_store_n(reinterpret_cast<u8*>(&value.current_state), static_cast<u8>(new_state),
                         __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool compare_state(thread& value, state expected, state desired) noexcept {
        u8 observed = static_cast<u8>(expected);
        return __atomic_compare_exchange_n(reinterpret_cast<u8*>(&value.current_state), &observed,
                                           static_cast<u8>(desired), false, __ATOMIC_ACQ_REL,
                                           __ATOMIC_ACQUIRE);
    }

    [[nodiscard]] inline bool runnable(const thread& value) noexcept {
        const state current = load_state(value);
        return current == state::ready || current == state::running;
    }

    [[nodiscard]] inline bool validate(const thread& value) noexcept {
        if (runnable(value)) {
            return value.context.instruction_pointer != 0U && value.context.stack_pointer != 0U;
        }
        return true;
    }

    inline void publish_pending(thread& value, pending_ipc kind, thread_id_t sender,
                                u32 sender_generation, capability::badge_t badge,
                                const word_t message[4]) noexcept {
        value.pending_sender = sender;
        value.pending_sender_generation = sender_generation;
        value.pending_badge = badge;
        for (usize_t index = 0U; index < 4U; ++index) {
            value.pending_message[index] = message[index];
        }
        __atomic_store_n(&value.pending_ipc_kind, static_cast<u8>(kind), __ATOMIC_RELEASE);
    }

    inline void consume_pending(thread& value) noexcept {
        const auto kind = static_cast<pending_ipc>(__atomic_exchange_n(
            &value.pending_ipc_kind, static_cast<u8>(pending_ipc::none), __ATOMIC_ACQUIRE));
        if (kind == pending_ipc::none) {
            if (value.pending_result != error_t::success) {
                value.context.x[0] = static_cast<word_t>(static_cast<s64>(value.pending_result));
                value.pending_result = error_t::success;
                value.ipc_timeout_active = false;
            }
            return;
        }

        value.context.x[0] = static_cast<word_t>(static_cast<s64>(value.pending_result));
        value.pending_result = error_t::success;
        value.ipc_timeout_active = false;
        if (kind == pending_ipc::incoming_call) {
            value.context.x[1] = static_cast<word_t>(value.pending_badge);
            for (usize_t index = 0U; index < 4U; ++index) {
                value.context.x[index + 2U] = value.pending_message[index];
            }
            if (!value.reply.valid) {
                value.reply.caller = value.pending_sender;
                value.reply.generation = value.pending_sender_generation;
                value.reply.nonce = value.pending_sender_generation;
                value.reply.donation_active = false;
                value.reply.valid = true;
            }
        } else {
            value.context.x[1] = static_cast<word_t>(value.pending_sender);
            for (usize_t index = 0U; index < 4U; ++index) {
                value.context.x[index + 2U] = value.pending_message[index];
            }
        }
        value.pending_sender = static_cast<thread_id_t>(-1);
        value.pending_sender_generation = 0U;
        value.pending_badge = 0U;
    }
} // namespace sys::kernel::thread
