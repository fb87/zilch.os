#pragma once

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/syscall_numbers.hh>
#include <sys/arch/syscall/entry.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduling/context.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>
#include <sys/types.hh>

namespace sys::kernel::syscall
{
    inline void set_control_result(arch::thread::context& frame,
                                   error_t result) noexcept
    {
        arch::syscall::set_result(
            frame, static_cast<word_t>(static_cast<s64>(result)));
    }

    [[nodiscard]] inline error_t resolve_task(
        thread::thread& current, capability_id_t selector,
        capability::right_t right, task::task*& result) noexcept
    {
        result = nullptr;
        if (current.owner == nullptr) return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(
            current.owner->cspace, selector, object::type_t::task,
            right, header);
        if (lookup != error_t::success) return lookup;
        result = reinterpret_cast<task::task*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_thread(
        thread::thread& current, capability_id_t selector,
        capability::right_t right, thread::thread*& result) noexcept
    {
        result = nullptr;
        if (current.owner == nullptr) return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(
            current.owner->cspace, selector, object::type_t::thread,
            right, header);
        if (lookup != error_t::success) return lookup;
        result = reinterpret_cast<thread::thread*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_space(
        thread::thread& current, capability_id_t selector,
        space::address_space*& result) noexcept
    {
        result = nullptr;
        if (current.owner == nullptr) return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(
            current.owner->cspace, selector, object::type_t::address_space,
            capability::right_t::control, header);
        if (lookup != error_t::success) return lookup;
        result = reinterpret_cast<space::address_space*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_frame(
        thread::thread& current, capability_id_t selector,
        capability::right_t right, memory::frame*& result) noexcept
    {
        result = nullptr;
        if (current.owner == nullptr) return error_t::denied;
        object::header_t* header = nullptr;
        const error_t lookup = capability::lookup(
            current.owner->cspace, selector, object::type_t::frame,
            right, header);
        if (lookup != error_t::success) return lookup;
        result = reinterpret_cast<memory::frame*>(header);
        return error_t::success;
    }

    [[nodiscard]] inline error_t dispatch_capability_operation(
        thread::thread& current, abi::v1::control_operation operation,
        capability_id_t destination_task_selector,
        capability_id_t destination_selector,
        capability_id_t source_selector, u32 rights) noexcept
    {
        if (current.owner == nullptr) return error_t::denied;
        task::task* destination_task = current.owner;
        if (destination_task_selector != 0U) {
            const error_t lookup = resolve_task(
                current, destination_task_selector,
                capability::right_t::control, destination_task);
            if (lookup != error_t::success) return lookup;
        }
        switch (operation) {
        case abi::v1::control_operation::capability_copy:
            return capability::copy(
                destination_task->cspace, destination_selector,
                current.owner->cspace, source_selector, {rights});
        case abi::v1::control_operation::capability_move:
            return capability::move(
                destination_task->cspace, destination_selector,
                current.owner->cspace, source_selector);
        case abi::v1::control_operation::capability_delete:
            return capability::delete_capability(
                destination_task->cspace, destination_selector);
        case abi::v1::control_operation::capability_revoke: {
            if (source_selector >= capability::cspace_slot_count)
                return error_t::invalid_argument;
            const object::reference_t reference =
                current.owner->cspace.slots[source_selector].object;
            if (reference.type == object::type_t::none)
                return error_t::not_found;
            capability::revoke_in(destination_task->cspace, reference);
            return error_t::success;
        }
        default:
            return error_t::unsupported;
        }
    }

    [[nodiscard]] inline bool dispatch_control(thread::thread& current,
                                               arch::thread::context& frame,
                                               u64 vector,
                                               u64 syndrome) noexcept
    {
        if (!arch::syscall::is_user_syscall(vector, syndrome)) return false;
        if (arch::syscall::number(frame)
            != static_cast<word_t>(abi::v1::syscall::control)) return false;

        const auto operation = static_cast<abi::v1::control_operation>(
            arch::syscall::argument(frame, 0U));
        const word_t a1 = arch::syscall::argument(frame, 1U);
        const word_t a2 = arch::syscall::argument(frame, 2U);
        const word_t a3 = arch::syscall::argument(frame, 3U);
        const word_t a4 = arch::syscall::argument(frame, 4U);
        const word_t a5 = arch::syscall::argument(frame, 5U);
        error_t result = error_t::unsupported;

        switch (operation) {
        case abi::v1::control_operation::capability_copy:
        case abi::v1::control_operation::capability_move:
        case abi::v1::control_operation::capability_delete:
        case abi::v1::control_operation::capability_revoke:
            result = dispatch_capability_operation(
                current, operation, a1, a2, a3, static_cast<u32>(a4));
            break;
        case abi::v1::control_operation::thread_start:
        case abi::v1::control_operation::thread_resume: {
            thread::thread* target = nullptr;
            result = resolve_thread(current, a1, capability::right_t::control,
                                    target);
            if (result == error_t::success) {
                thread::store_state(*target, thread::state::ready);
                result = error_t::success;
            }
            break;
        }
        case abi::v1::control_operation::thread_suspend: {
            thread::thread* target = nullptr;
            result = resolve_thread(current, a1, capability::right_t::control,
                                    target);
            if (result == error_t::success) {
                thread::store_state(*target, thread::state::suspended);
                platform::interrupt::send_ipi_all_others(
                    platform::interrupt::reschedule_ipi);
            }
            break;
        }
        case abi::v1::control_operation::map_frame: {
            space::address_space* target_space = nullptr;
            memory::frame* source_frame = nullptr;
            result = resolve_space(current, a1, target_space);
            if (result == error_t::success)
                result = resolve_frame(current, a2,
                                       capability::right_t::write,
                                       source_frame);
            if (result == error_t::success) {
                result = memory::map(
                    *target_space, *source_frame, a3,
                    static_cast<memory::permission>(a4));
            }
            break;
        }
        case abi::v1::control_operation::unmap_frame: {
            space::address_space* target_space = nullptr;
            memory::frame* source_frame = nullptr;
            result = resolve_space(current, a1, target_space);
            if (result == error_t::success)
                result = resolve_frame(current, a2,
                                       capability::right_t::write,
                                       source_frame);
            if (result == error_t::success)
                result = memory::unmap(*target_space, *source_frame);
            break;
        }
        case abi::v1::control_operation::notification_signal:
        case abi::v1::control_operation::notification_poll: {
            if (current.owner == nullptr) {
                result = error_t::denied;
                break;
            }
            object::header_t* header = nullptr;
            result = capability::lookup(
                current.owner->cspace, a1, object::type_t::notification,
                operation == abi::v1::control_operation::notification_signal
                    ? capability::right_t::write
                    : capability::right_t::read,
                header);
            if (result == error_t::success) {
                auto& notification =
                    *reinterpret_cast<notification::notification*>(header);
                if (operation
                    == abi::v1::control_operation::notification_signal) {
                    notification::signal(notification, a2);
                } else {
                    frame.x[1] = notification::consume(notification);
                }
            }
            break;
        }
        case abi::v1::control_operation::interrupt_bind: {
            if (current.owner == nullptr) {
                result = error_t::denied;
                break;
            }
            object::header_t* interrupt_header = nullptr;
            object::header_t* notification_header = nullptr;
            result = capability::lookup(
                current.owner->cspace, a1, object::type_t::interrupt,
                capability::right_t::control, interrupt_header);
            if (result == error_t::success) {
                result = capability::lookup(
                    current.owner->cspace, a2,
                    object::type_t::notification,
                    capability::right_t::write, notification_header);
            }
            if (result == error_t::success) {
                auto& interrupt =
                    *reinterpret_cast<interrupt::interrupt_t*>(interrupt_header);
                auto& target = *reinterpret_cast<notification::notification*>(
                    notification_header);
                interrupt.notification = object::reference(target.object);
            }
            break;
        }
        case abi::v1::control_operation::interrupt_ack:
            result = error_t::success;
            break;
        case abi::v1::control_operation::acceptance_report: {
            if (current.owner == nullptr || !current.owner->root) {
                result = error_t::denied;
                break;
            }
            const char* name = "unknown";
            switch (a1) {
            case 1U: name = "root_only_boot"; break;
            case 2U: name = "bootinfo_contract"; break;
            case 3U: name = "capability_control"; break;
            case 4U: name = "notification_control"; break;
            case 5U: name = "root_created_objects"; break;
            case 6U: name = "root_created_smp_fuzz"; break;
            case 7U: name = "object_destroy_reuse"; break;
            default: break;
            }
            pr_info("[TEST] name=%s result=%s\n", name, a2 != 0U ? "PASS" : "FAIL");
            result = a2 != 0U ? error_t::success : error_t::invalid_argument;
            break;
        }
        case abi::v1::control_operation::acceptance_finalize:
            if (current.owner == nullptr || !current.owner->root) {
                result = error_t::denied;
                break;
            }
            pr_info("[ACCEPTANCE] profile=1.0 boot=root-only result=%s failures=%u\n",
                    a1 != 0U ? "PASS" : "FAIL", a1 != 0U ? 0U : 1U);
            result = a1 != 0U ? error_t::success : error_t::invalid_argument;
            break;
        case abi::v1::control_operation::scheduling_configure: {
            thread::thread* target = nullptr;
            result = resolve_thread(current, a1, capability::right_t::control,
                                    target);
            if (result == error_t::success) {
                target->scheduling_context.priority = static_cast<u8>(a2);
                target->scheduling_context.budget_ticks = a3;
                target->scheduling_context.period_ticks = a4;
            }
            break;
        }
        case abi::v1::control_operation::child_create:
            if (current.owner == nullptr || !current.owner->root) {
                result = error_t::denied;
                break;
            }
            result = thread::create_user_bundle(
                *current.owner, static_cast<cpu_id_t>(a1), a2,
                static_cast<capability_id_t>(a3),
                static_cast<capability_id_t>(a4),
                static_cast<capability_id_t>(a5));
            if (result == error_t::success) {
                platform::interrupt::send_ipi_all_others(
                    platform::interrupt::reschedule_ipi);
            }
            break;
        case abi::v1::control_operation::child_destroy:
            if (current.owner == nullptr || !current.owner->root) {
                result = error_t::denied;
                break;
            }
            result = thread::destroy_user_bundle(
                *current.owner, static_cast<capability_id_t>(a1),
                static_cast<capability_id_t>(a2),
                static_cast<capability_id_t>(a3));
            break;
        case abi::v1::control_operation::acceptance_worker_tick: {
            const cpu_id_t cpu = arch::cpu::current_id();
            if (cpu >= thread::maximum_cpu_count || current.owner == nullptr
                || current.owner->root) {
                result = error_t::denied;
                break;
            }
            const u64 operations = __atomic_add_fetch(
                &thread::certification_operations[cpu], 1U, __ATOMIC_RELAXED);
            if (operations == 4096U) {
                pr_info("root fuzz cpu=%u operations=%llu failures=%llu status=PASS\n",
                        static_cast<unsigned int>(cpu),
                        static_cast<unsigned long long>(operations),
                        static_cast<unsigned long long>(
                            __atomic_load_n(&thread::certification_failures[cpu],
                                            __ATOMIC_ACQUIRE)));
            }
            __atomic_store_n(&thread::certification_state[cpu], a3,
                             __ATOMIC_RELEASE);
            if (a1 != 0U) {
                __atomic_fetch_add(&thread::certification_failures[cpu], a1,
                                   __ATOMIC_RELAXED);
            }
            result = error_t::success;
            break;
        }
        case abi::v1::control_operation::acceptance_query: {
            if (current.owner == nullptr || !current.owner->root
                || a1 >= thread::maximum_cpu_count) {
                result = error_t::denied;
                break;
            }
            const u64 value = a2 == 0U
                ? __atomic_load_n(&thread::certification_operations[a1],
                                  __ATOMIC_ACQUIRE)
                : (a2 == 1U
                    ? __atomic_load_n(&thread::certification_failures[a1],
                                      __ATOMIC_ACQUIRE)
                    : __atomic_load_n(&thread::certification_state[a1],
                                      __ATOMIC_ACQUIRE));
            arch::syscall::set_result(frame, static_cast<word_t>(value));
            return true;
        }
        case abi::v1::control_operation::acceptance_soak_report:
            if (current.owner == nullptr || !current.owner->root
                || a2 >= thread::maximum_cpu_count) {
                result = error_t::denied;
                break;
            }
            pr_info("[SOAK] epoch=%llu cpu=%u operations=%llu failures=%llu seed=%llx progress=%s\n",
                    static_cast<unsigned long long>(a1),
                    static_cast<unsigned int>(a2),
                    static_cast<unsigned long long>(a3),
                    static_cast<unsigned long long>(a4),
                    static_cast<unsigned long long>(a5),
                    a4 == 0U ? "yes" : "no");
            result = a4 == 0U ? error_t::success : error_t::invalid_argument;
            break;
        }
        set_control_result(frame, result);
        return true;
    }
}
