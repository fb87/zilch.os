#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/platform/platform.hh>
#include <sys/platform/timer.hh>

namespace sys::kernel::interrupt
{
    inline constexpr u32 maximum_irq_count = 1020U;
    inline constexpr u32 storm_threshold = 64U;
    inline constexpr u64 storm_window_ticks = 100U;

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
    };

    inline interrupt_t* registry[maximum_irq_count]{};

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
        if (value.irq >= maximum_irq_count || registry[value.irq] != nullptr)
            return error_t::busy;
        registry[value.irq] = &value;
        platform::interrupt::mask(value.irq);
        return platform::interrupt::configure(value.irq, value.trigger_mode == trigger::edge);
    }

    inline void unregister_irq(interrupt_t& value) noexcept {
        platform::interrupt::mask(value.irq);
        if (value.irq < maximum_irq_count && registry[value.irq] == &value)
            registry[value.irq] = nullptr;
        value.notification = {};
        value.masked = true;
        value.active = false;
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
            platform::interrupt::unmask(value.irq);
        }
        return error_t::success;
    }

    [[nodiscard]] inline bool dispatch(irq_id_t irq) noexcept {
        if (irq >= maximum_irq_count)
            return false;
        interrupt_t* value = registry[irq];
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
} // namespace sys::kernel::interrupt
