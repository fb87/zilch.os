#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/task/task.hh>
#include <sys/platform/platform.hh>
#include <sys/platform/timer.hh>

namespace sys::kernel::interrupt
{
    inline constexpr u32 maximum_irq_count = 1020U;
    inline constexpr u32 storm_threshold = 64U;
    inline constexpr u64 storm_window_ticks = 100U;
    inline constexpr u32 dynamic_interrupt_count = 16U;

    enum class trigger : u8 {
        level,
        edge,
    };

    struct interrupt_t {
        object::header_t object{};
        irq_id_t irq{};
        object::reference_t notification{};
        trigger trigger_mode{trigger::level};
        volatile bool masked{true};
        volatile bool active{};
        volatile bool stormed{};
        volatile u64 delivered{};
        volatile u64 acknowledged{};
        volatile u64 suppressed{};
        volatile u64 window_start{};
        volatile u32 window_count{};
        volatile u32 allocated{};
    };

    inline interrupt_t* registry[maximum_irq_count]{};
    inline interrupt_t dynamic_interrupts[dynamic_interrupt_count]{};

    inline void initialize(interrupt_t& value, irq_id_t irq,
                           trigger mode = trigger::level) noexcept {
        value.irq = irq;
        value.notification = {};
        value.trigger_mode = mode;
        value.masked = true;
        value.active = false;
        value.stormed = false;
        value.delivered = 0U;
        value.acknowledged = 0U;
        value.suppressed = 0U;
        value.window_start = 0U;
        value.window_count = 0U;
    }

    [[nodiscard]] inline error_t register_irq(interrupt_t& value) noexcept {
        if (value.irq >= maximum_irq_count || !platform::interrupt::userspace_assignable(value.irq))
            return error_t::busy;
        interrupt_t* expected = nullptr;
        if (!__atomic_compare_exchange_n(&registry[value.irq], &expected, &value, false,
                                         __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            return error_t::busy;
        platform::interrupt::mask(value.irq);
        const error_t result =
            platform::interrupt::configure(value.irq, value.trigger_mode == trigger::edge);
        if (result != error_t::success) {
            expected = &value;
            (void)__atomic_compare_exchange_n(&registry[value.irq], &expected, nullptr, false,
                                              __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        }
        return result;
    }

    inline void unregister_irq(interrupt_t& value) noexcept {
        platform::interrupt::mask(value.irq);
        if (value.irq < maximum_irq_count) {
            interrupt_t* expected = &value;
            (void)__atomic_compare_exchange_n(&registry[value.irq], &expected, nullptr, false,
                                              __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        }
        value.notification = {};
        value.masked = true;
        value.active = false;
    }

    /*
     * Lets a root-privileged task mint an interrupt capability for a
     * userspace-assignable IRQ at runtime, mirroring notification::create().
     * This is what a domain-manager-style server uses to own a physical
     * device interrupt generically, instead of the kernel/hypervisor
     * hardcoding a specific device's IRQ number.
     */
    [[nodiscard]] inline error_t create(task::task& owner, capability_id_t selector, irq_id_t irq,
                                        trigger mode = trigger::level) noexcept {
        if (!owner.root)
            return error_t::denied;
        if (selector >= capability::cspace_slot_count)
            return error_t::invalid_argument;
        if (capability::slot_at(owner.cspace, selector).object.type != object::type_t::none)
            return error_t::busy;
        for (u32 index = 0U; index < dynamic_interrupt_count; ++index) {
            interrupt_t& value = dynamic_interrupts[index];
            u32 expected = 0U;
            if (!__atomic_compare_exchange_n(&value.allocated, &expected, 1U, false,
                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                continue;
            initialize(value, irq, mode);
            error_t result = object::register_dynamic_object(value.object, object::type_t::interrupt);
            if (result == error_t::success)
                result = register_irq(value);
            if (result == error_t::success) {
                result =
                    capability::install(owner.cspace, selector, object::reference(value.object),
                                        {static_cast<u32>(capability::right_t::read) |
                                         static_cast<u32>(capability::right_t::write) |
                                         static_cast<u32>(capability::right_t::grant) |
                                         static_cast<u32>(capability::right_t::control)});
            }
            if (result != error_t::success) {
                unregister_irq(value);
                if (value.object.type != object::type_t::none)
                    (void)object::unregister_object(object::reference(value.object));
                value.object = {};
                __atomic_store_n(&value.allocated, 0U, __ATOMIC_RELEASE);
            }
            return result;
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline bool record_delivery(interrupt_t& value, u64 now) noexcept {
        if (now - __atomic_load_n(&value.window_start, __ATOMIC_ACQUIRE) >= storm_window_ticks) {
            __atomic_store_n(&value.window_start, now, __ATOMIC_RELEASE);
            __atomic_store_n(&value.window_count, 0U, __ATOMIC_RELEASE);
        }
        const u32 count = __atomic_add_fetch(&value.window_count, 1U, __ATOMIC_ACQ_REL);
        if (count > storm_threshold) {
            __atomic_store_n(&value.stormed, true, __ATOMIC_RELEASE);
            __atomic_store_n(&value.masked, true, __ATOMIC_RELEASE);
            __atomic_add_fetch(&value.suppressed, 1U, __ATOMIC_RELAXED);
            platform::interrupt::mask(value.irq);
            return false;
        }
        if (__atomic_load_n(&value.masked, __ATOMIC_ACQUIRE) ||
            __atomic_load_n(&value.active, __ATOMIC_ACQUIRE)) {
            __atomic_add_fetch(&value.suppressed, 1U, __ATOMIC_RELAXED);
            return false;
        }
        __atomic_store_n(&value.active, true, __ATOMIC_RELEASE);
        __atomic_store_n(&value.masked, true, __ATOMIC_RELEASE);
        __atomic_add_fetch(&value.delivered, 1U, __ATOMIC_RELAXED);
        platform::interrupt::mask(value.irq);
        return true;
    }

    [[nodiscard]] inline error_t bind(interrupt_t& value,
                                      const object::reference_t& target) noexcept {
        object::header_t* header = object::resolve(target);
        if (header == nullptr || header->type != object::type_t::notification)
            return error_t::invalid_argument;
        platform::interrupt::mask(value.irq);
        __atomic_store_n(&value.masked, true, __ATOMIC_RELEASE);
        if (__atomic_load_n(&value.active, __ATOMIC_ACQUIRE))
            return error_t::busy;
        value.notification = target;
        __atomic_store_n(&value.stormed, false, __ATOMIC_RELEASE);
        __atomic_store_n(&value.window_count, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&value.masked, false, __ATOMIC_RELEASE);
        platform::interrupt::unmask(value.irq);
        return error_t::success;
    }

    [[nodiscard]] inline error_t acknowledge(interrupt_t& value) noexcept {
        bool expected = true;
        if (!__atomic_compare_exchange_n(&value.active, &expected, false, false, __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE))
            return error_t::not_found;
        platform::interrupt::deactivate(value.irq);
        __atomic_add_fetch(&value.acknowledged, 1U, __ATOMIC_RELAXED);
        if (!__atomic_load_n(&value.stormed, __ATOMIC_ACQUIRE)) {
            __atomic_store_n(&value.masked, false, __ATOMIC_RELEASE);
            platform::interrupt::route_to_current_cpu(value.irq);
            platform::interrupt::unmask(value.irq);
        }
        return error_t::success;
    }

    [[nodiscard]] inline bool dispatch(irq_id_t irq) noexcept {
        if (irq >= maximum_irq_count)
            return false;
        interrupt_t* value = __atomic_load_n(&registry[irq], __ATOMIC_ACQUIRE);
        if (value == nullptr ||
            !record_delivery(*value, platform::timer::ticks(arch::cpu::current_id())))
            return false;
        object::header_t* header = object::resolve(value->notification);
        if (header == nullptr || header->type != object::type_t::notification) {
            __atomic_store_n(&value->stormed, true, __ATOMIC_RELEASE);
            __atomic_store_n(&value->active, false, __ATOMIC_RELEASE);
            return false;
        }
        auto& target = *reinterpret_cast<notification::notification*>(header);
        notification::signal(target, 1ULL << (irq & 63U));
        return true;
    }

    [[nodiscard]] inline bool database_valid() noexcept {
        for (u32 irq = 0U; irq < maximum_irq_count; ++irq) {
            interrupt_t* value = __atomic_load_n(&registry[irq], __ATOMIC_ACQUIRE);
            if (value == nullptr)
                continue;
            if (value->irq != irq || value->object.type != object::type_t::interrupt ||
                __atomic_load_n(&value->acknowledged, __ATOMIC_ACQUIRE) >
                    __atomic_load_n(&value->delivered, __ATOMIC_ACQUIRE) ||
                (__atomic_load_n(&value->active, __ATOMIC_ACQUIRE) &&
                 !__atomic_load_n(&value->masked, __ATOMIC_ACQUIRE)) ||
                (__atomic_load_n(&value->stormed, __ATOMIC_ACQUIRE) &&
                 !__atomic_load_n(&value->masked, __ATOMIC_ACQUIRE)))
                return false;
            if (value->notification.type != object::type_t::none &&
                object::resolve(value->notification) == nullptr)
                return false;
        }
        return true;
    }
} // namespace sys::kernel::interrupt
