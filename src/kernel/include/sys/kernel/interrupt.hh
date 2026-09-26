#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/emergency.hh>
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
    /*
     * One past the highest IRQ ever registered, so the per-tick sweep in
     * recover_stormed() costs a handful of loads instead of walking all
     * 1020 registry slots every tick on every boot. Only ever grows;
     * unregister_irq() deliberately does not shrink it, since a slot that
     * was used once can be reused and the bound only needs to be an
     * over-approximation to stay correct.
     */
    inline volatile u32 registry_bound{};

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
        for (u32 bound = __atomic_load_n(&registry_bound, __ATOMIC_ACQUIRE);
             value.irq >= bound &&
             !__atomic_compare_exchange_n(&registry_bound, &bound, value.irq + 1U, false,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);) {
        }
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
        /*
         * An outstanding delivery normally means the current owner still
         * owes an acknowledge, and rebinding under it would strand that --
         * so refuse.
         *
         * Unless the owner is GONE. If the notification this line was bound
         * to no longer resolves, the task holding it was destroyed with a
         * delivery in flight, and nothing will ever acknowledge it: `active`
         * stays set for the remaining uptime and every future bind fails
         * with busy. That made a driver unrestartable precisely when it
         * most needed restarting -- after crashing while servicing its own
         * interrupt -- and was the real obstacle behind USR-024, rather
         * than the client re-minting that item assumed (clients hold
         * capabilities to the endpoint OBJECT, which root owns and which
         * outlives the task).
         *
         * Taking the line over deactivates it at the controller first, so
         * the fresh owner starts from a clean GIC state rather than
         * inheriting a half-serviced one.
         */
        if (__atomic_load_n(&value.active, __ATOMIC_ACQUIRE)) {
            object::header_t* previous = object::resolve(value.notification);
            if (previous != nullptr && previous->type == object::type_t::notification)
                return error_t::busy;
            platform::interrupt::deactivate(value.irq);
            __atomic_store_n(&value.active, false, __ATOMIC_RELEASE);
        }
        value.notification = target;
        __atomic_store_n(&value.stormed, false, __ATOMIC_RELEASE);
        __atomic_store_n(&value.window_count, 0U, __ATOMIC_RELEASE);
        __atomic_store_n(&value.masked, false, __ATOMIC_RELEASE);
        platform::interrupt::unmask(value.irq);
        // OBS-008: a device line changing hands is an assignment event.
        emergency::append(emergency::event::device_assign, value.irq);
        return error_t::success;
    }

    /*
     * Severs a line from its owner: masked at the controller, unbound, and
     * with any in-flight delivery dropped.
     *
     * Revoking an interrupt capability used to remove only the cspace entry.
     * The interrupt object stayed bound to the old owner's notification and
     * stayed unmasked, so the device kept firing into a driver that no
     * longer owned it -- the name was revoked, the authority was not, which
     * is not revocation at all (DEV-003). Root re-delegating the line calls
     * bind() again, which re-arms it.
     *
     * Idempotent, and safe on a line that was never bound: every step is a
     * store to a known value or a controller operation that tolerates
     * repetition.
     */
    inline void release_ownership(interrupt_t& value) noexcept {
        platform::interrupt::mask(value.irq);
        __atomic_store_n(&value.masked, true, __ATOMIC_RELEASE);
        if (__atomic_exchange_n(&value.active, false, __ATOMIC_ACQ_REL))
            platform::interrupt::deactivate(value.irq);
        value.notification = {};
        __atomic_store_n(&value.stormed, false, __ATOMIC_RELEASE);
        __atomic_store_n(&value.window_count, 0U, __ATOMIC_RELEASE);
        // OBS-008: authority withdrawn, whether by revoke or teardown.
        emergency::append(emergency::event::device_revoke, value.irq);
    }

    /*
     * Installed into the capability layer so a revoke severs device
     * authority, not just the name. Filters by type here rather than there:
     * cspace.hh has no business knowing which object types own hardware.
     */
    inline void on_capability_revoked(const object::reference_t& reference) noexcept {
        object::header_t* header = object::resolve(reference);
        if (header == nullptr || header->type != object::type_t::interrupt)
            return;
        release_ownership(*reinterpret_cast<interrupt_t*>(header));
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

    /*
     * Re-arms lines the storm detector masked, once their detection window
     * has fully elapsed. A storm is a RATE limit, not a death sentence:
     * `stormed` used to be cleared only by bind(), while acknowledge()
     * refuses to unmask while it is set, so any line that ever crossed
     * storm_threshold stayed masked for the remaining uptime. The only
     * recovery was for the owner to notice and re-bind, which no driver
     * does. A single burst of legitimate traffic -- a fast typist on the
     * console, a busy disk -- permanently killed the device.
     *
     * MUST be driven by the timer, and this is the load-bearing part:
     * while `stormed` holds the line masked at the GIC, nothing is
     * delivered, so the owner has nothing left to acknowledge and an
     * acknowledge-driven re-arm can never run at all. That was the obvious
     * place to put it and it is wrong -- the interrupt lifecycle test
     * caught it immediately, because after the storm the next
     * record_delivery() is suppressed by the very mask being recovered
     * from. Nothing inside the interrupt path can break that cycle; only
     * an external clock can.
     *
     * Re-storming is intended: a line that really is stuck asserting trips
     * the detector again after another storm_threshold deliveries, so the
     * steady state is a ceiling of roughly storm_threshold per
     * storm_window_ticks. Containment is preserved; permanence is not.
     *
     * `active` is respected rather than overridden: a delivery in flight
     * belongs to acknowledge(), which owns that unmask.
     */
    inline void recover_stormed(u64 now) noexcept {
        /*
         * Walks the registry rather than the dynamic_interrupts pool:
         * record_delivery() and dispatch() can set `stormed` on ANY
         * registered interrupt, whichever storage it lives in, so keying
         * recovery off one pool would leave the others latched. Bounded by
         * registry_bound so the common case is a few loads.
         */
        const u32 bound = __atomic_load_n(&registry_bound, __ATOMIC_ACQUIRE);
        for (u32 irq = 0U; irq < bound; ++irq) {
            interrupt_t* const value = __atomic_load_n(&registry[irq], __ATOMIC_ACQUIRE);
            if (value == nullptr || !__atomic_load_n(&value->stormed, __ATOMIC_ACQUIRE))
                continue;
            if (now - __atomic_load_n(&value->window_start, __ATOMIC_ACQUIRE) < storm_window_ticks)
                continue;
            __atomic_store_n(&value->window_start, now, __ATOMIC_RELEASE);
            __atomic_store_n(&value->window_count, 0U, __ATOMIC_RELEASE);
            __atomic_store_n(&value->stormed, false, __ATOMIC_RELEASE);
            if (!__atomic_load_n(&value->active, __ATOMIC_ACQUIRE)) {
                __atomic_store_n(&value->masked, false, __ATOMIC_RELEASE);
                platform::interrupt::unmask(value->irq);
            }
        }
    }

    /*
     * `target`/`badge` let the caller do the actual signal, rather than
     * dispatch() doing it here: waking a thread bound to that notification
     * needs thread:: facilities (scheduler.hh's signal_notification()),
     * and this file cannot depend on scheduler.hh without a genuine
     * circular include -- scheduler.hh already depends on bootstrap.hh,
     * which depends on this file. dispatch()'s one real caller
     * (src/arch/arm64/arch.cc) already includes scheduler.hh directly, so
     * it is the natural place to finish the signal.
     */
    struct dispatch_result final {
        bool delivered{};
        notification::notification* target{};
        u64 badge{};
    };

    [[nodiscard]] inline dispatch_result dispatch(irq_id_t irq) noexcept {
        if (irq >= maximum_irq_count)
            return {};
        interrupt_t* value = __atomic_load_n(&registry[irq], __ATOMIC_ACQUIRE);
        if (value == nullptr ||
            !record_delivery(*value, platform::timer::ticks(arch::cpu::current_id())))
            return {};
        object::header_t* header = object::resolve(value->notification);
        if (header == nullptr || header->type != object::type_t::notification) {
            __atomic_store_n(&value->stormed, true, __ATOMIC_RELEASE);
            __atomic_store_n(&value->active, false, __ATOMIC_RELEASE);
            return {};
        }
        return {true, reinterpret_cast<notification::notification*>(header),
                1ULL << (irq & 63U)};
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
