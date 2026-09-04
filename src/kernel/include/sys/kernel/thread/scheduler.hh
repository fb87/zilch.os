#pragma once
#include <sys/arch/cpu.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/entry.hh>
#include <sys/kernel/boot/bootinfo.hh>
#include <sys/kernel/bootstrap.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/hypervisor.hh>
#include <sys/kernel/interrupt/timing.hh>
#include <sys/kernel/ipc/endpoint.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/panic.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/kernel/user_access.hh>
#include <sys/kernel/verification/hooks.hh>
#include <sys/platform/timer.hh>
#include <sys/types.hh>

namespace sys::kernel::thread
{
    /*
     * 12 (the maximum the pinning log's old, now-removed per-CPU-3 assert
     * ever allowed) left exactly one usable slot of headroom over the
     * guest+block production graph's actual consumption of 10: memory-
     * server, serial-driver, virtio-driver, five control_plane_role
     * launches, console-server's stdin thread, and root's supervision
     * thread. That assert was never a real scheduling constraint -- see
     * log_cpu_assignment()'s comment -- so there was no principled reason
     * to stay that close to the edge. 16 gives room for the service graph
     * to grow by more than one thread before this needs revisiting; no
     * other structure in the kernel sizes itself off this constant (only
     * scheduler.hh, syscall/ipc.hh, syscall/control.hh, and kernel.hh's
     * boot log consume it, all as a loop bound, not a fixed-size peer
     * array), and it is a .bss array, not a stack one, so growing it costs
     * static kernel image size, not stack budget.
     */
    inline constexpr u32 user_thread_count = 16U;
    inline constexpr u32 maximum_cpu_count = 4U;
    inline u32 active_user_thread_count = CONFIG_ROOT_ONLY_BOOT ? 1U : user_thread_count;
    inline constexpr u64 fault_timeout_ticks = 500U;

    struct timeout_entry {
        thread_id_t thread{};
        u32 generation{};
        u64 deadline{};
    };

    inline timeout_entry timeout_queues[maximum_cpu_count][user_thread_count]{};
    inline u32 timeout_queue_counts[maximum_cpu_count]{};
    inline volatile u32 timeout_queue_locks[maximum_cpu_count]{};

    [[nodiscard]] inline interrupt::timing::state lock_timeout_queue(cpu_id_t cpu) noexcept {
        const interrupt::timing::state state = interrupt::timing::save_and_disable();
        while (__atomic_exchange_n(&timeout_queue_locks[cpu], 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
        lock_order::acquired(lock_order::rank::scheduler_timeout, &timeout_queue_locks[cpu]);
        return state;
    }

    inline void unlock_timeout_queue(cpu_id_t cpu, interrupt::timing::state state) noexcept {
        lock_order::released(lock_order::rank::scheduler_timeout, &timeout_queue_locks[cpu]);
        __atomic_store_n(&timeout_queue_locks[cpu], 0U, __ATOMIC_RELEASE);
        interrupt::timing::restore(state);
    }

    inline void arm_ipc_timeout(thread& value) noexcept {
        if (!value.ipc_timeout_active || value.pinned_cpu >= maximum_cpu_count)
            return;
        const cpu_id_t cpu = value.pinned_cpu;
        const interrupt::timing::state irq_state = lock_timeout_queue(cpu);
        u32& count = timeout_queue_counts[cpu];
        for (u32 index = 0U; index < count;) {
            if (timeout_queues[cpu][index].thread == value.id) {
                for (u32 move = index + 1U; move < count; ++move)
                    timeout_queues[cpu][move - 1U] = timeout_queues[cpu][move];
                --count;
            } else {
                ++index;
            }
        }
        if (count < user_thread_count) {
            u32 position = count;
            while (position > 0U &&
                   timeout_queues[cpu][position - 1U].deadline > value.ipc_deadline) {
                timeout_queues[cpu][position] = timeout_queues[cpu][position - 1U];
                --position;
            }
            timeout_queues[cpu][position] = {value.id, value.object.generation, value.ipc_deadline};
            ++count;
        }
        unlock_timeout_queue(cpu, irq_state);
    }

    inline void expire_ipc_timeouts(cpu_id_t cpu, u64 now) noexcept;
    [[nodiscard]] inline bool deliver_fault_ipc(thread& value, arch::thread::context& frame,
                                                u64 syndrome, vaddr_t fault_address,
                                                fault::kind fault_kind) noexcept;

    [[nodiscard]] inline constexpr fault::kind classify_user_fault(u64 exception_class) noexcept {
        if (exception_class == 0x00U || exception_class == 0x20U || exception_class == 0x21U)
            return fault::kind::instruction_abort;
        if (exception_class == 0x22U || exception_class == 0x26U)
            return fault::kind::alignment;
        if (exception_class == 0x24U || exception_class == 0x25U)
            return fault::kind::data_abort;
        return fault::kind::none;
    }
    inline constexpr word_t memory_server_role = 0x100U;
    inline constexpr word_t fault_client_role = 0x101U;
    inline constexpr word_t ipc_lifecycle_server_role = 0x113U;
    inline constexpr word_t capability_race_server_role = 0x115U;
    inline thread user_threads[user_thread_count]{};
    inline task::task user_tasks[user_thread_count]{};
    inline u32 current_user_thread[maximum_cpu_count]{};
    inline u32 current_user_generation[maximum_cpu_count]{};
    inline bool user_execution_active[maximum_cpu_count]{};
    inline bool user_cpu_idle[maximum_cpu_count]{};
    inline volatile bool user_scheduler_ready = false;
    extern "C" [[noreturn]] void sys_kernel_user_idle() noexcept;
    inline volatile u64 per_cpu_switches[maximum_cpu_count]{};

    [[nodiscard]] inline constexpr capability::badge_t
    endpoint_badge(const thread& value) noexcept {
        return (static_cast<capability::badge_t>(value.object.generation) << 32U) |
               static_cast<capability::badge_t>(value.id + 1U);
    }

    [[nodiscard]] inline error_t initialize_user_threads() noexcept {
        error_t bootstrap_result = bootstrap::initialize_objects();
        if (bootstrap_result != error_t::success)
            return bootstrap_result;

        for (u32 index = 0U; index < ipc::endpoint_count; ++index) {
            ipc::initialize(ipc::endpoints[index]);
            const error_t result = object::register_object(
                ipc::endpoints[index].object,
                static_cast<object_id_t>(object::bootstrap_id::endpoint_base + index),
                object::type_t::endpoint);
            if (result != error_t::success)
                return result;
        }

        for (u32 id = 0U; id < active_user_thread_count; ++id) {
            task::initialize(user_tasks[id], static_cast<space_id_t>(id));
            error_t result = object::register_object(
                user_tasks[id].object,
                static_cast<object_id_t>(object::bootstrap_id::task_base + id),
                object::type_t::task);
            if (result != error_t::success)
                return result;

            result = initialize_user(
                user_threads[id], static_cast<thread_id_t>(id),
                static_cast<cpu_id_t>(id % maximum_cpu_count),
                CONFIG_ROOT_ONLY_BOOT ? 0U : static_cast<word_t>(id),
                CONFIG_ROOT_ONLY_BOOT ? 0U : static_cast<word_t>(initial_fuzz_seed(id)));
            if (result != error_t::success)
                return result;
            user_threads[id].owner = &user_tasks[id];
            user_tasks[id].fault_endpoint = 10U;
            result = object::register_object(
                user_threads[id].object,
                static_cast<object_id_t>(object::bootstrap_id::thread_base + id),
                object::type_t::thread);
            if (result != error_t::success)
                return result;

            result = object::register_object(
                user_threads[id].address_space.object,
                static_cast<object_id_t>(object::bootstrap_id::address_space_base + id),
                object::type_t::address_space);
            if (result != error_t::success)
                return result;
            result = object::register_object(
                user_threads[id].scheduling_context.object,
                static_cast<object_id_t>(object::bootstrap_id::scheduling_context_base + id),
                object::type_t::scheduling_context);
            if (result != error_t::success)
                return result;

            user_tasks[id].root = id == 0U;
            result = capability::install(user_tasks[id].cspace, 1U,
                                         object::reference(user_tasks[id].object),
                                         {static_cast<u32>(capability::right_t::read) |
                                          static_cast<u32>(capability::right_t::control)});
            if (result != error_t::success)
                return result;
            result = capability::install(user_tasks[id].cspace, 2U,
                                         object::reference(user_threads[id].object),
                                         {static_cast<u32>(capability::right_t::read) |
                                          static_cast<u32>(capability::right_t::write) |
                                          static_cast<u32>(capability::right_t::control)});
            if (result != error_t::success)
                return result;
            result = capability::install(user_tasks[id].cspace, 3U,
                                         object::reference(user_threads[id].address_space.object),
                                         {static_cast<u32>(capability::right_t::read) |
                                          static_cast<u32>(capability::right_t::write) |
                                          static_cast<u32>(capability::right_t::control)});
            if (result != error_t::success)
                return result;
            result =
                capability::install(user_tasks[id].cspace, 4U,
                                    object::reference(user_threads[id].scheduling_context.object),
                                    {static_cast<u32>(capability::right_t::read) |
                                     static_cast<u32>(capability::right_t::control)});
            if (result != error_t::success)
                return result;

            const capability::rights_t client_rights =
                capability::rights(capability::right_t::write);
            result = capability::install(
                user_tasks[id].cspace, 10U, object::reference(ipc::endpoints[0].object),
                id == 0U ? capability::rights_t{static_cast<u32>(capability::right_t::read) |
                                                static_cast<u32>(capability::right_t::write) |
                                                static_cast<u32>(capability::right_t::grant) |
                                                static_cast<u32>(capability::right_t::control)}
                         : client_rights);
            if (result != error_t::success)
                return result;
            result = capability::install(
                user_tasks[id].cspace, 11U, object::reference(ipc::endpoints[1].object),
                id == 1U ? capability::rights_t{static_cast<u32>(capability::right_t::read) |
                                                static_cast<u32>(capability::right_t::write) |
                                                static_cast<u32>(capability::right_t::grant) |
                                                static_cast<u32>(capability::right_t::control)}
                         : client_rights);
            if (result != error_t::success)
                return result;

            store_state(user_threads[id], state::ready);
        }
        task::task& root_task = user_tasks[0];
        error_t root_result =
            memory::create_resource(root_task, 32U, memory::managed_pages, {}, true);
        if (root_result != error_t::success)
            return root_result;
        const capability::rights_t root_memory_rights{
            static_cast<u32>(capability::right_t::read) |
            static_cast<u32>(capability::right_t::write) |
            static_cast<u32>(capability::right_t::grant) |
            static_cast<u32>(capability::right_t::control)};
        root_result = capability::install(
            root_task.cspace, 12U, object::reference(memory::frames[0].object), root_memory_rights);
        if (root_result != error_t::success)
            return root_result;
        root_result = capability::install(root_task.cspace, 13U,
                                          object::reference(memory::page_tables[0].object),
                                          root_memory_rights);
        if (root_result != error_t::success)
            return root_result;
        root_result = capability::install(root_task.cspace, 14U,
                                          object::reference(bootstrap::root_notification.object),
                                          {static_cast<u32>(capability::right_t::read) |
                                           static_cast<u32>(capability::right_t::write) |
                                           static_cast<u32>(capability::right_t::grant) |
                                           static_cast<u32>(capability::right_t::control)});
        if (root_result != error_t::success)
            return root_result;
        root_result = capability::install(root_task.cspace, 15U,
                                          object::reference(bootstrap::root_timer_interrupt.object),
                                          {static_cast<u32>(capability::right_t::read) |
                                           static_cast<u32>(capability::right_t::write) |
                                           static_cast<u32>(capability::right_t::control)});
        if (root_result != error_t::success)
            return root_result;
        if constexpr (arch::hypervisor::active) {
            root_result = capability::install(root_task.cspace, 28U,
                                              object::reference(hypervisor::bootstrap_vm.object),
                                              {static_cast<u32>(capability::right_t::read) |
                                               static_cast<u32>(capability::right_t::write) |
                                               static_cast<u32>(capability::right_t::grant) |
                                               static_cast<u32>(capability::right_t::control)});
            if (root_result != error_t::success)
                return root_result;
            root_result = capability::install(root_task.cspace, 29U,
                                              object::reference(hypervisor::bootstrap_vcpu.object),
                                              {static_cast<u32>(capability::right_t::read) |
                                               static_cast<u32>(capability::right_t::write) |
                                               static_cast<u32>(capability::right_t::control)});
            if (root_result != error_t::success)
                return root_result;
        }

        boot::root_bootinfo.cpu_count = maximum_cpu_count;
        boot::root_bootinfo.root_task = 1U;
        boot::root_bootinfo.root_thread = 2U;
        boot::root_bootinfo.root_space = 3U;
        boot::root_bootinfo.root_fault_endpoint = 10U;
        boot::root_bootinfo.capability_count = arch::hypervisor::active ? 11U : 9U;
        const capability_id_t selectors[11]{1U, 2U, 3U, 4U, 10U, 12U, 14U, 15U, 32U, 28U, 29U};
        for (u32 index = 0U; index < boot::root_bootinfo.capability_count; ++index) {
            const capability::slot_t& slot =
                capability::slot_at(root_task.cspace, selectors[index]);
            boot::root_bootinfo.capabilities[index] = {selectors[index], slot.object,
                                                       slot.rights.bits};
        }
        boot::root_bootinfo.memory_region_count = memory::physical_region_count;
        boot::root_bootinfo.memory_page_size = static_cast<u32>(memory::page_size);
        boot::root_bootinfo.memory_total_pages = memory::managed_pages;
        boot::root_bootinfo.memory_free_pages =
            __atomic_load_n(&memory::free_pages, __ATOMIC_ACQUIRE);
        for (u32 index = 0U; index < boot::maximum_memory_region_count; ++index) {
            if (index < memory::physical_region_count) {
                const auto& region = memory::physical_regions[index];
                boot::root_bootinfo.memory_regions[index] = {
                    region.base, region.pages,
                    region.allocatable ? boot::memory_region_allocatable : 0U, 0U};
            } else {
                boot::root_bootinfo.memory_regions[index] = {};
            }
        }

        for (u32 cpu = 0U; cpu < maximum_cpu_count; ++cpu) {
            current_user_thread[cpu] = CONFIG_ROOT_ONLY_BOOT ? 0U : cpu;
            current_user_generation[cpu] = 0U;
            user_execution_active[cpu] = false;
            user_cpu_idle[cpu] = false;
            per_cpu_switches[cpu] = 0U;
            timeout_queue_counts[cpu] = 0U;
            timeout_queue_locks[cpu] = 0U;
        }
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_fault_with_frame(thread& pager, thread& target,
                                                          memory::frame& source, vaddr_t address,
                                                          memory::permission permissions) noexcept {
        lock_ipc_lifecycle();
        if (load_state(target) != state::blocked_fault ||
            target.fault_disposition != fault::disposition::pending) {
            unlock_ipc_lifecycle();
            return error_t::invalid_argument;
        }
        if (target.owner == nullptr || pager.owner == nullptr) {
            unlock_ipc_lifecycle();
            return error_t::denied;
        }
        const vaddr_t page_address = address & ~(memory::page_size - 1U);
        const vaddr_t fault_page = target.last_fault.address & ~(memory::page_size - 1U);
        const error_t validation =
            fault::validate_mapping_reply(target.last_fault, page_address, fault_page, permissions);
        if (validation != error_t::success) {
            unlock_ipc_lifecycle();
            return validation;
        }
        error_t result =
            memory::map(target.address_space, source, page_address, permissions, 0U, 0U);
        if (result == error_t::busy &&
            memory::mapping_present(target.address_space, page_address, permissions))
            result = error_t::success;
        if (result != error_t::success) {
            unlock_ipc_lifecycle();
            return result;
        }
        target.fault_disposition = fault::disposition::resume;
        target.last_fault = {};
        target.waiting_endpoint = 0U;
        target.ipc_timeout_active = false;
        if (!compare_state(target, state::blocked_fault, state::ready)) {
            unlock_ipc_lifecycle();
            return error_t::busy;
        }
        unlock_ipc_lifecycle();
        ipc::remote_reschedule(target.pinned_cpu, arch::cpu::current_id());
        return error_t::success;
    }

    inline volatile u64 certification_operations[maximum_cpu_count]{};
    inline volatile u64 certification_failures[maximum_cpu_count]{};

    [[nodiscard]] inline u32 find_free_user_slot(cpu_id_t preferred) noexcept {
        if (preferred > 0U && preferred < user_thread_count &&
            load_state(user_threads[preferred]) == state::inactive)
            return preferred;
        for (u32 id = 1U; id < user_thread_count; ++id) {
            if (load_state(user_threads[id]) == state::inactive)
                return id;
        }
        return user_thread_count;
    }

    inline void clear_user_bundle(thread& target, task::task& owner) noexcept {
        target.address_space.release(&memory::release_physical_page);
        arch::thread::clear(target.context);
        for (usize_t index = 0U; index < 4U; ++index) {
            target.message[index] = 0U;
            target.pending_message[index] = 0U;
        }
        target.pending_sender = static_cast<thread_id_t>(-1);
        target.pending_sender_generation = 0U;
        target.pending_badge = 0U;
        target.ipc_badge = 0U;
        target.pending_ipc_kind = static_cast<u8>(pending_ipc::none);
        target.pending_result = error_t::success;
        target.ipc_deadline = 0U;
        target.reports = 0U;
        target.fuzz_seed = 0U;
        target.fuzz_iterations = 0U;
        target.fuzz_failures = 0U;
        target.faults = 0U;
        target.scheduling_context.priority = 0U;
        target.scheduling_context.effective_priority = 0U;
        target.scheduling_context.maximum_priority = 0U;
        target.scheduling_context.donation_depth = 0U;
        target.scheduling_context.budget_ticks = 0U;
        target.scheduling_context.period_ticks = 0U;
        target.scheduling_context.consumed_ticks = 0U;
        target.scheduling_context.donated_ticks = 0U;
        target.scheduling_context.next_replenishment = 0U;
        target.scheduling_context.affinity = 0U;
        target.scheduling_context.enabled = false;
        target.scheduling_context.throttled = false;
        target.scheduling_context.replenishment_count = 0U;
        target.owner = nullptr;
        target.object = {};
        target.address_space.object = {};
        target.scheduling_context.object = {};
        target.waiting_endpoint = 0U;
        target.reply = {};
        clear_transfer(target.transfer);
        target.ipc_timeout_active = false;
        target.last_fault = {};
        target.fault_disposition = fault::disposition::pending;
        store_state(target, state::inactive);
        capability::initialize(owner.cspace);
        owner.object = {};
        owner.address_space_id = 0U;
        owner.fault_endpoint = 0U;
        owner.memory_quota_pages = 64U;
        owner.memory_pages_owned = 0U;
        owner.root = false;
    }

    [[nodiscard]] inline error_t create_user_bundle(task::task& root, cpu_id_t cpu, word_t role,
                                                    capability_id_t thread_selector,
                                                    capability_id_t task_selector,
                                                    capability_id_t space_selector,
                                                    u32* allocated_id = nullptr) noexcept {
        if (!root.root || cpu >= maximum_cpu_count ||
            thread_selector >= capability::cspace_slot_count ||
            task_selector >= capability::cspace_slot_count ||
            space_selector >= capability::cspace_slot_count || thread_selector == task_selector ||
            thread_selector == space_selector || task_selector == space_selector) {
            return error_t::invalid_argument;
        }
        if (capability::slot_at(root.cspace, thread_selector).object.type != object::type_t::none ||
            capability::slot_at(root.cspace, task_selector).object.type != object::type_t::none ||
            capability::slot_at(root.cspace, space_selector).object.type != object::type_t::none)
            return error_t::busy;

        const u32 id = find_free_user_slot(cpu);
        if (id >= user_thread_count)
            return error_t::no_memory;

        task::task& owner = user_tasks[id];
        thread& target = user_threads[id];
        task::initialize(owner, static_cast<space_id_t>(id));
        error_t result =
            initialize_user(target, static_cast<thread_id_t>(id), cpu, role, initial_fuzz_seed(id));
        if (result != error_t::success)
            return result;
        target.owner = &owner;
        owner.fault_endpoint = 10U;

        result = object::register_dynamic_object(owner.object, object::type_t::task);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.object, object::type_t::thread);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.address_space.object,
                                                     object::type_t::address_space);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.scheduling_context.object,
                                                     object::type_t::scheduling_context);

        const capability::rights_t control_rights{static_cast<u32>(capability::right_t::read) |
                                                  static_cast<u32>(capability::right_t::write) |
                                                  static_cast<u32>(capability::right_t::grant) |
                                                  static_cast<u32>(capability::right_t::control)};
        if (result == error_t::success)
            result = capability::install(root.cspace, thread_selector,
                                         object::reference(target.object), control_rights);
        if (result == error_t::success)
            result = capability::install(root.cspace, task_selector,
                                         object::reference(owner.object), control_rights);
        if (result == error_t::success)
            result =
                capability::install(root.cspace, space_selector,
                                    object::reference(target.address_space.object), control_rights);
        if (result == error_t::success)
            result = capability::install(owner.cspace, 1U, object::reference(owner.object),
                                         control_rights);
        if (result == error_t::success)
            result = capability::install(owner.cspace, 2U, object::reference(target.object),
                                         control_rights);
        if (result == error_t::success)
            result = capability::install(
                owner.cspace, 3U, object::reference(target.address_space.object), control_rights);
        if (result == error_t::success)
            result = memory::delegate_resource(root, memory::resources[0], owner, 15U,
                                               owner.memory_quota_pages);
        if (result == error_t::success)
            result = capability::install(owner.cspace, 4U,
                                         object::reference(target.scheduling_context.object),
                                         control_rights);
        if (result == error_t::success) {
            capability::rights_t endpoint_rights = capability::rights(capability::right_t::write);
            if (role == memory_server_role || role == ipc_lifecycle_server_role ||
                role == capability_race_server_role) {
                endpoint_rights.bits |= static_cast<u32>(capability::right_t::read);
            }
            result = capability::mint(owner.cspace, 10U, root.cspace, 10U, endpoint_rights,
                                      endpoint_badge(target));
        }
        if (result == error_t::success)
            result = capability::install(owner.cspace, 14U,
                                         object::reference(bootstrap::root_notification.object),
                                         capability::rights(capability::right_t::write));

        if (result != error_t::success) {
            if (capability::slot_at(owner.cspace, 15U).object.type ==
                object::type_t::memory_resource)
                (void)memory::destroy_resource(owner, 15U);
            (void)capability::delete_capability(root.cspace, thread_selector);
            (void)capability::delete_capability(root.cspace, task_selector);
            (void)capability::delete_capability(root.cspace, space_selector);
            capability::revoke_reference(object::reference(target.scheduling_context.object));
            capability::revoke_reference(object::reference(target.address_space.object));
            capability::revoke_reference(object::reference(target.object));
            capability::revoke_reference(object::reference(owner.object));
            if (target.scheduling_context.object.type != object::type_t::none)
                (void)object::unregister_object(
                    object::reference(target.scheduling_context.object));
            if (target.address_space.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target.address_space.object));
            if (target.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target.object));
            if (owner.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(owner.object));
            clear_user_bundle(target, owner);
            return result;
        }

        if (id >= active_user_thread_count)
            __atomic_store_n(&active_user_thread_count, id + 1U, __ATOMIC_RELEASE);
        /*
         * Indexed by cpu, NOT by id: these two arrays are sized
         * [maximum_cpu_count] (4) and every reader in the certification
         * harness indexes them by CPU, but id is a thread-slot id ranging
         * over user_thread_count (10). Indexing them by id wrote zeros out
         * of bounds for any id >= 4, landing directly on the globals that
         * follow them in .bss -- memory::mapping_lock, object::table_lock,
         * and the first bytes of ipc::endpoints[0].object, whose zeroed
         * generation field then made root's own fault-endpoint capability
         * (root cspace slot 10) fail object::resolve(). cpu is already
         * bounds-checked against maximum_cpu_count at function entry, and
         * matches what the harness's own acceptance_query(cpu) reads back.
         */
        certification_operations[cpu] = 0U;
        certification_failures[cpu] = 0U;

        store_state(target, state::ready);
        if (allocated_id != nullptr)
            *allocated_id = id;
        return error_t::success;
    }

    /*
     * Duplicates the calling process into a new task, thread and address
     * space: POSIX fork.
     *
     * Differs from create_user_bundle() in what fills the new space and
     * cspace. There is no role and no ELF load -- the image, stack and
     * frame-backed mappings are copied from the parent -- and the child's
     * cspace starts as a copy of the parent's rather than the fixed
     * bootstrap set, so it inherits every capability the parent held. The
     * four self-referring slots (task, thread, space, scheduling context)
     * are then overwritten to name the child's own objects, and the fault
     * endpoint is re-minted with the child's badge so faults are
     * attributable to it and not to its parent.
     *
     * The copy is eager: there is no copy-on-write, so a fork costs one
     * physical page per mapped page up front. That is the honest cost of
     * having no fault-driven sharing, and it is why fork is bounded by the
     * frame pool.
     */
    [[nodiscard]] inline error_t fork_user_bundle(thread& parent,
                                                  const arch::thread::context& parent_frame,
                                                  capability_id_t child_selector,
                                                  u32* allocated_id) noexcept {
        if (parent.owner == nullptr)
            return error_t::denied;
        if (child_selector >= capability::cspace_slot_count)
            return error_t::invalid_argument;
        if (capability::slot_at(parent.owner->cspace, child_selector).object.type !=
            object::type_t::none)
            return error_t::busy;
        const u32 id = find_free_user_slot(parent.pinned_cpu);
        if (id >= user_thread_count)
            return error_t::no_memory;

        task::task& owner = user_tasks[id];
        thread& target = user_threads[id];
        task::initialize(owner, static_cast<space_id_t>(id));
        /*
         * The child lands on the next CPU rather than its parent's.
         *
         * There is no yield syscall here, so a parent waiting on a child
         * spins through process_wait -- and every capability operation takes
         * a global authority lock, so a parent spinning on its child's own
         * CPU starves exactly the thread it is waiting for. Keeping POSIX's
         * usual affinity would make the common fork-then-wait pattern
         * livelock rather than merely run slowly.
         */
        const cpu_id_t child_cpu =
            static_cast<cpu_id_t>((parent.pinned_cpu + 1U) % maximum_cpu_count);
        /*
         * Role 0 with no fuzz seed: initialize_user() would load role 0's
         * image, which fork must not do, so the image it loads is discarded
         * immediately below by clone(). Going through it anyway keeps every
         * other field reset in exactly one place.
         */
        error_t result = initialize_user(target, static_cast<thread_id_t>(id), child_cpu, 0U, 0U);
        if (result != error_t::success)
            return result;
        target.owner = &owner;
        owner.fault_endpoint = parent.owner->fault_endpoint;
        owner.memory_quota_pages = parent.owner->memory_quota_pages;

        result = arch::space::clone(target.address_space.native, parent.address_space.native,
                                    &memory::allocate_physical_page,
                                    &memory::release_physical_page);

        if (result == error_t::success)
            result = object::register_dynamic_object(owner.object, object::type_t::task);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.object, object::type_t::thread);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.address_space.object,
                                                     object::type_t::address_space);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.scheduling_context.object,
                                                     object::type_t::scheduling_context);

        const capability::rights_t control_rights{static_cast<u32>(capability::right_t::read) |
                                                  static_cast<u32>(capability::right_t::write) |
                                                  static_cast<u32>(capability::right_t::grant) |
                                                  static_cast<u32>(capability::right_t::control)};

        /*
         * Inherit the parent's capabilities, then correct the ones that must
         * name the child. Slots 1-4 and 10 are overwritten below; 15 is the
         * parent's memory resource, which is deliberately shared rather than
         * re-delegated, so a forked child draws from the same quota its
         * parent was granted instead of silently doubling it.
         */
        for (capability_id_t selector = 0U;
             result == error_t::success && selector < capability::cspace_slot_count; ++selector) {
            const capability::slot_t& source = capability::slot_at(parent.owner->cspace, selector);
            if (source.object.type == object::type_t::none)
                continue;
            result = capability::inherit(owner.cspace, selector, parent.owner->cspace, selector);
        }

        const auto replace = [&](capability_id_t selector,
                                 object::reference_t reference) noexcept -> error_t {
            (void)capability::delete_capability(owner.cspace, selector);
            return capability::install(owner.cspace, selector, reference, control_rights);
        };
        if (result == error_t::success)
            result = replace(1U, object::reference(owner.object));
        if (result == error_t::success)
            result = replace(2U, object::reference(target.object));
        if (result == error_t::success)
            result = replace(3U, object::reference(target.address_space.object));
        if (result == error_t::success)
            result = replace(4U, object::reference(target.scheduling_context.object));
        if (result == error_t::success) {
            const capability::slot_t& fault = capability::slot_at(parent.owner->cspace, 10U);
            if (fault.object.type != object::type_t::none) {
                (void)capability::delete_capability(owner.cspace, 10U);
                /*
                 * inherit_badged, not mint: mint derives, and derivation
                 * requires the grant right, which a write-only fault
                 * endpoint deliberately lacks.
                 */
                result = capability::inherit_badged(owner.cspace, 10U, parent.owner->cspace, 10U,
                                                    endpoint_badge(target));
            }
        }
        if (result == error_t::success)
            result = memory::clone_mappings(target.address_space, parent.address_space, owner);
        /*
         * Installed into the PARENT, after the child's cspace was copied, so
         * the child does not also receive a capability to itself here -- it
         * already has one at slot 2. Without this a parent could not wait on
         * its own child: fork reports a thread id, and process_wait takes a
         * capability precisely so a stale reference fails closed.
         */
        if (result == error_t::success)
            result = capability::install(parent.owner->cspace, child_selector,
                                         object::reference(target.object), control_rights);

        if (result != error_t::success) {
            (void)capability::delete_capability(parent.owner->cspace, child_selector);
            capability::revoke_reference(object::reference(target.scheduling_context.object));
            capability::revoke_reference(object::reference(target.address_space.object));
            capability::revoke_reference(object::reference(target.object));
            capability::revoke_reference(object::reference(owner.object));
            memory::unmap_all(target.address_space);
            memory::reclaim_task_memory(object::reference(owner.object));
            target.address_space.release(&memory::release_physical_page);
            if (target.scheduling_context.object.type != object::type_t::none)
                (void)object::unregister_object(
                    object::reference(target.scheduling_context.object));
            if (target.address_space.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target.address_space.object));
            if (target.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target.object));
            if (owner.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(owner.object));
            clear_user_bundle(target, owner);
            return result;
        }

        /*
         * The child resumes at the same instruction as the parent, on a copy
         * of its register state, distinguished only by fork's return value.
         */
        /* Field-wise, not a struct assignment: the kernel is built
         * -fno-builtin with no memcpy to lower an aggregate copy onto. */
        arch::thread::copy(target.context, parent_frame);
        /*
         * fork reports 0 in the child and the child's thread id in the
         * parent. Zero is unambiguous: slot 0 is the boot root thread, which
         * is occupied from bring-up and never exits, so find_free_user_slot()
         * cannot return it.
         */
        arch::syscall::set_output(target.context, 0U, static_cast<word_t>(error_t::success));
        arch::syscall::set_output(target.context, 1U, 0U);

        if (id >= active_user_thread_count)
            __atomic_store_n(&active_user_thread_count, id + 1U, __ATOMIC_RELEASE);
        store_state(target, state::ready);
        if (allocated_id != nullptr)
            *allocated_id = id;
        return error_t::success;
    }

    /*
     * Replaces the calling process's image with the one bound to `role`,
     * in place: POSIX exec.
     *
     * The task, cspace and thread identity all survive; only the address
     * space is rebuilt. That is what makes fork-then-exec work -- a child
     * can arrange its capabilities (redirecting a standard stream, say) and
     * then replace its code without losing them.
     *
     * Failure is not recoverable in the usual sense. Once the old address
     * space has been torn down there is nothing to return to, so a failed
     * load leaves the thread with image_status set and terminates it rather
     * than resuming a process whose code has been unmapped. The one case
     * that IS recoverable is a role with no image bound, which is checked
     * before anything is destroyed.
     */
    [[nodiscard]] inline error_t exec_user_image(thread& current, arch::thread::context& frame,
                                                 word_t role) noexcept {
        if (current.owner == nullptr)
            return error_t::denied;
        /* Checked first, while the caller is still intact and can be told. */
        const auto image = arch::space::image_for_role(role);
        if (image.end <= image.start)
            return error_t::not_found;

        /*
         * Drop the old address space's frame-backed mappings and pages
         * before the space itself is reinitialised. reclaim_task_memory()
         * is what returns the frames a heap was built from; without it an
         * exec would leak every page the previous image had mapped, which
         * is the same defect USR-034 found on the destroy path.
         */
        memory::unmap_all(current.address_space);
        memory::reclaim_task_memory(object::reference(current.owner->object));

        /*
         * Switch to the permanent kernel root before rebuilding this space.
         *
         * Kernel execution currently shares TTBR0 with each user address
         * space, and initialize() rewrites l0/l1/l2 in place -- so doing it
         * while TTBR0 still points at them wipes the translation backing the
         * kernel's own instruction stream, mid-syscall, on the CPU doing the
         * rewriting. activate_kernel() exists for precisely this hazard on
         * the teardown path; exec is the first caller that rebuilds a live
         * space rather than retiring it.
         */
        arch::space::activate_kernel();

        const error_t result = current.address_space.initialize(
            static_cast<space_id_t>(current.id), role, &memory::allocate_physical_page,
            &memory::release_physical_page);
        if (result != error_t::success) {
            store_state(current, state::terminated);
            return result;
        }
        current.address_space.activate();
        arch::thread::initialize_user(frame, arch::space::entry(current.address_space.native),
                                      arch::space::stack_top(), role, 0U);
        return error_t::success;
    }

    /*
     * Unlike create_user_bundle(), this attaches a fresh thread to the
     * CALLER's own EXISTING task instead of allocating a new one -- no
     * task_selector, no root-gating (you can only ever add a thread to
     * yourself, never inject one into another task, so there's no new
     * privilege-escalation surface the way process_create's root check
     * guards against). The new thread shares owner's cspace (and thus its
     * root flag and every capability already in it) automatically, since
     * capability resolution always goes through current.owner->cspace --
     * nothing extra to wire up for that. It gets its own independent
     * address_space (own page tables, loaded from role's bound image),
     * same as any other thread; address_space is thread-owned, not
     * task-owned, in this object model, so "sharing a task" here means
     * sharing capabilities/authority, not memory.
     *
     * user_tasks[]/user_threads[] share one user_thread_count-sized pool
     * with no separate free list for tasks vs threads (find_free_user_slot
     * only inspects user_threads[]); this consumes one of those slots and
     * the paired user_tasks[id] at that index goes permanently unused.
     * Accepted, documented waste given the pool is small and this is only
     * ever used to give root a second thread.
     */
    [[nodiscard]] inline error_t create_user_thread(task::task& owner, cpu_id_t cpu, word_t role,
                                                    capability_id_t thread_selector,
                                                    capability_id_t space_selector) noexcept {
        if (cpu >= maximum_cpu_count || thread_selector >= capability::cspace_slot_count ||
            space_selector >= capability::cspace_slot_count || thread_selector == space_selector) {
            return error_t::invalid_argument;
        }
        if (capability::slot_at(owner.cspace, thread_selector).object.type !=
                object::type_t::none ||
            capability::slot_at(owner.cspace, space_selector).object.type != object::type_t::none)
            return error_t::busy;

        const u32 id = find_free_user_slot(cpu);
        if (id >= user_thread_count)
            return error_t::no_memory;

        thread& target = user_threads[id];
        error_t result =
            initialize_user(target, static_cast<thread_id_t>(id), cpu, role, initial_fuzz_seed(id));
        if (result != error_t::success)
            return result;
        target.owner = &owner;

        result = object::register_dynamic_object(target.object, object::type_t::thread);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.address_space.object,
                                                     object::type_t::address_space);
        if (result == error_t::success)
            result = object::register_dynamic_object(target.scheduling_context.object,
                                                     object::type_t::scheduling_context);

        const capability::rights_t control_rights{static_cast<u32>(capability::right_t::read) |
                                                  static_cast<u32>(capability::right_t::write) |
                                                  static_cast<u32>(capability::right_t::grant) |
                                                  static_cast<u32>(capability::right_t::control)};
        if (result == error_t::success)
            result = capability::install(owner.cspace, thread_selector,
                                         object::reference(target.object), control_rights);
        if (result == error_t::success)
            result =
                capability::install(owner.cspace, space_selector,
                                    object::reference(target.address_space.object), control_rights);

        if (result != error_t::success) {
            (void)capability::delete_capability(owner.cspace, thread_selector);
            (void)capability::delete_capability(owner.cspace, space_selector);
            capability::revoke_reference(object::reference(target.scheduling_context.object));
            capability::revoke_reference(object::reference(target.address_space.object));
            capability::revoke_reference(object::reference(target.object));
            if (target.scheduling_context.object.type != object::type_t::none)
                (void)object::unregister_object(
                    object::reference(target.scheduling_context.object));
            if (target.address_space.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target.address_space.object));
            if (target.object.type != object::type_t::none)
                (void)object::unregister_object(object::reference(target.object));
            /*
             * Deliberately NOT clear_user_bundle(): that also wipes the
             * OWNER task (capability::initialize(owner.cspace), root =
             * false, quota reset, ...) -- correct when tearing down a
             * bundle this function itself allocated the task for, wrong
             * here since owner is the caller's own live task. Only the
             * new thread's own state is rolled back.
             */
            target.address_space.release(&memory::release_physical_page);
            arch::thread::clear(target.context);
            target.owner = nullptr;
            target.object = {};
            target.address_space.object = {};
            target.scheduling_context.object = {};
            store_state(target, state::inactive);
            return result;
        }

        if (id >= active_user_thread_count)
            __atomic_store_n(&active_user_thread_count, id + 1U, __ATOMIC_RELEASE);
        // Indexed by cpu, not id -- see create_user_bundle() above.
        certification_operations[cpu] = 0U;
        certification_failures[cpu] = 0U;

        store_state(target, state::ready);
        return error_t::success;
    }

    [[nodiscard]] inline error_t quiesce_user_thread(thread& target) noexcept {
        const state previous_state = load_state(target);
        if (target.owner != nullptr && target.waiting_endpoint < capability::cspace_slot_count &&
            (previous_state == state::blocked_send || previous_state == state::blocked_receive)) {
            object::header_t* endpoint_header = nullptr;
            const capability::right_t endpoint_right = previous_state == state::blocked_receive
                                                           ? capability::right_t::read
                                                           : capability::right_t::write;
            if (capability::lookup(target.owner->cspace, target.waiting_endpoint,
                                   object::type_t::endpoint, endpoint_right,
                                   endpoint_header) == error_t::success &&
                endpoint_header != nullptr) {
                auto& endpoint = *reinterpret_cast<ipc::endpoint*>(endpoint_header);
                (void)ipc::cancel_thread(endpoint, object::reference(target.object));
            }
        }

        lock_ipc_lifecycle();
        store_state(target, state::suspended);
        for (u32 server_index = 0U; server_index < active_user_thread_count; ++server_index) {
            thread& server = user_threads[server_index];
            if (server.reply.valid && server.reply.caller == target.id &&
                server.reply.generation == target.object.generation) {
                if (server.reply.donation_active)
                    scheduling::revoke_donation(server.scheduling_context,
                                                target.scheduling_context);
                server.reply = {};
            }
        }
        target.ipc_timeout_active = false;
        clear_transfer(target.transfer);
        target.ipc_badge = 0U;
        target.pending_ipc_kind = static_cast<u8>(pending_ipc::none);
        unlock_ipc_lifecycle();
        platform::interrupt::send_ipi(target.pinned_cpu, platform::interrupt::reschedule_ipi);

        const u32 target_generation = target.object.generation;
        constexpr u32 maximum_wait_rounds = 1000000U;
        for (u32 round = 0U; round < maximum_wait_rounds; ++round) {
            bool cpu_bound = false;
            for (u32 cpu = 0U; cpu < maximum_cpu_count; ++cpu) {
                if (__atomic_load_n(&current_user_thread[cpu], __ATOMIC_ACQUIRE) == target.id &&
                    __atomic_load_n(&current_user_generation[cpu], __ATOMIC_ACQUIRE) ==
                        target_generation) {
                    cpu_bound = true;
                    break;
                }
            }
            if (!cpu_bound && !__atomic_load_n(&target.executing, __ATOMIC_ACQUIRE))
                return error_t::success;
            if ((round & 0x3ffU) == 0U) {
                platform::interrupt::send_ipi(target.pinned_cpu,
                                              platform::interrupt::reschedule_ipi);
            }
            arch::cpu::relax();
        }
        return error_t::busy;
    }

    // Defined below, alongside the rest of the scheduling/blocking helpers.
    [[nodiscard]] inline bool wake(thread& value) noexcept;

    /*
     * If target owes someone a reply (target.reply.valid: it is a server
     * that a caller is blocked_reply/blocked_fault waiting on), wake that
     * caller instead of leaving it stuck forever. This logic already existed
     * inline in the thread_exit syscall handler for a *graceful* exit, but a
     * *forced* destroy (process_destroy -> destroy_user_bundle) skipped it
     * entirely -- nothing else clears or wakes a caller waiting on a thread
     * that gets torn down out from under it. Never exercised before because
     * nothing previously called process_destroy on a task that owed anyone a
     * reply; restart-on-fault (USR-034) does exactly that when it destroys a
     * crashed role that root's blocked ipc_call is waiting on.
     */
    inline void release_pending_reply(thread& target) noexcept {
        lock_ipc_lifecycle();
        if (target.reply.valid && target.reply.caller < user_thread_count) {
            thread& caller = user_threads[target.reply.caller];
            const state caller_state = load_state(caller);
            if (caller.object.generation == target.reply.generation) {
                if (caller_state == state::blocked_reply) {
                    caller.ipc_timeout_active = false;
                    clear_transfer(caller.transfer);
                    caller.pending_result = error_t::timed_out;
                    (void)wake(caller);
                    ipc::remote_reschedule(caller.pinned_cpu, arch::cpu::current_id());
                } else if (caller_state == state::blocked_fault) {
                    caller.ipc_timeout_active = false;
                    caller.waiting_endpoint = 0U;
                    caller.fault_disposition = fault::disposition::terminate;
                    store_state(caller, state::terminated);
                }
            }
            if (target.reply.donation_active)
                scheduling::revoke_donation(target.scheduling_context, caller.scheduling_context);
            target.reply = {};
        }
        unlock_ipc_lifecycle();
    }

    [[nodiscard]] inline error_t destroy_user_bundle(task::task& root,
                                                     capability_id_t thread_selector,
                                                     capability_id_t task_selector,
                                                     capability_id_t space_selector) noexcept {
        object::header_t* header = nullptr;
        error_t result = capability::lookup(root.cspace, thread_selector, object::type_t::thread,
                                            capability::right_t::control, header);
        if (result != error_t::success)
            return result;
        auto& target = *reinterpret_cast<thread*>(header);
        task::task* owner = target.owner;
        if (target.id == 0U || target.id >= user_thread_count)
            return error_t::denied;
        result = quiesce_user_thread(target);
        if (result != error_t::success)
            return result;
        release_pending_reply(target);

        /*
         * Tear down every OTHER thread that shares this task first. Since
         * thread_create, a task can own more than the one thread named by
         * thread_selector (create_user_thread() attaches additional threads
         * to an existing task), and destroying only the named one left the
         * siblings alive pointing at a task object that was about to be
         * unregistered and reused -- a dangling owner the certification
         * harness's own process_lifecycle_valid() correctly rejects, and a
         * thread that would keep getting scheduled against freed state.
         * Each sibling owns its own address_space and scheduling_context
         * (thread-owned, not task-owned) so each needs its own release;
         * the cspace itself is shared and is torn down once, below, with
         * the task.
         */
        for (u32 sibling = 0U; sibling < active_user_thread_count; ++sibling) {
            thread& other = user_threads[sibling];
            if (&other == &target || other.owner != owner ||
                other.object.type != object::type_t::thread)
                continue;
            result = quiesce_user_thread(other);
            if (result != error_t::success)
                return result;
            release_pending_reply(other);
            const object::reference_t other_scheduling =
                object::reference(other.scheduling_context.object);
            const object::reference_t other_space = object::reference(other.address_space.object);
            const object::reference_t other_thread = object::reference(other.object);
            {
                const capability::authority_guard authority_transaction{};
                store_state(other, state::terminated);
                memory::unmap_all(other.address_space);
                capability::revoke_reference_locked(other_thread);
                capability::revoke_reference_locked(other_space);
                capability::revoke_reference_locked(other_scheduling);
            }
            (void)object::unregister_object(other_scheduling);
            (void)object::unregister_object(other_space);
            (void)object::unregister_object(other_thread);
            other.address_space.release(&memory::release_physical_page);
            arch::thread::clear(other.context);
            other.owner = nullptr;
            other.object = {};
            other.address_space.object = {};
            other.scheduling_context.object = {};
            other.waiting_endpoint = 0U;
            other.reply = {};
            clear_transfer(other.transfer);
            other.ipc_timeout_active = false;
            other.last_fault = {};
            other.fault_disposition = fault::disposition::pending;
            store_state(other, state::inactive);
        }

        if (capability::slot_at(owner->cspace, 15U).object.type ==
            object::type_t::memory_resource) {
            result = memory::destroy_resource(*owner, 15U);
            if (result != error_t::success)
                return result;
        }

        const object::reference_t scheduling_reference =
            object::reference(target.scheduling_context.object);
        const object::reference_t space_reference = object::reference(target.address_space.object);
        const object::reference_t thread_reference = object::reference(target.object);
        const object::reference_t task_reference = object::reference(owner->object);
        {
            const capability::authority_guard authority_transaction{};
            header = nullptr;
            result = capability::lookup(root.cspace, thread_selector, object::type_t::thread,
                                        capability::right_t::control, header);
            if (result != error_t::success || header != &target.object)
                return result == error_t::success ? error_t::not_found : result;
            result = capability::lookup(root.cspace, task_selector, object::type_t::task,
                                        capability::right_t::control, header);
            if (result != error_t::success || header != &owner->object)
                return result == error_t::success ? error_t::not_found : result;
            result = capability::lookup(root.cspace, space_selector, object::type_t::address_space,
                                        capability::right_t::control, header);
            if (result != error_t::success || header != &target.address_space.object)
                return result == error_t::success ? error_t::not_found : result;
            store_state(target, state::terminated);
            memory::unmap_all(target.address_space);
            (void)capability::revoke_all_locked(
                owner->cspace, [](capability::derivation_id_t derivation) noexcept {
                    (void)memory::unmap_authority(derivation, false);
                });
            capability::revoke_reference_locked(thread_reference);
            capability::revoke_reference_locked(task_reference);
            capability::revoke_reference_locked(space_reference);
            capability::revoke_reference_locked(scheduling_reference);
        }
        /*
         * After the authority transaction (this reclaims via the unlocked
         * capability/object entry points) and after unmap_all() above, so
         * the task's own mappings are already gone and its frames are
         * releasable. Without this every frame and page table the task
         * allocated and did not explicitly destroy leaks its pool slot --
         * see reclaim_task_memory().
         */
        memory::reclaim_task_memory(task_reference);
        result = object::unregister_object(scheduling_reference);
        if (result == error_t::success)
            result = object::unregister_object(space_reference);
        if (result == error_t::success)
            result = object::unregister_object(thread_reference);
        if (result == error_t::success)
            result = object::unregister_object(task_reference);
        if (result != error_t::success)
            return result;
        clear_user_bundle(target, *owner);
        return error_t::success;
    }

    /*
     * Tears down a bundle process_fork created, using only the thread
     * capability the parent already holds. Structurally the same teardown
     * as destroy_user_bundle() -- sibling threads first, then the
     * memory-resource slot, then revoke/unregister/reclaim -- but reached
     * differently: destroy_user_bundle() re-validates thread/task/space
     * selectors against root's OWN cspace, because root can independently
     * hold three separate capabilities into the same bundle. A forking
     * parent never receives task or space capabilities at all (see
     * fork_user_bundle()), only the thread's, so there is nothing to
     * re-validate them against -- the thread object's own owner/
     * address_space/scheduling_context links are the only path to the rest
     * of the bundle, the same links the sibling-teardown loop already uses
     * without a cspace lookup.
     *
     * Requiring state::terminated (process_wait's "exited") rather than
     * quiescing a running thread here is deliberate: a parent that has not
     * waited yet has no proof the child is done touching its own memory,
     * and tearing down a live thread's address space out from under it is
     * a different, more dangerous operation than reaping one that already
     * reported its exit status.
     */
    [[nodiscard]] inline error_t reap_user_bundle(thread& current,
                                                  capability_id_t thread_selector) noexcept {
        if (current.owner == nullptr)
            return error_t::denied;
        object::header_t* header = nullptr;
        error_t result = capability::lookup(current.owner->cspace, thread_selector,
                                            object::type_t::thread, capability::right_t::control,
                                            header);
        if (result != error_t::success)
            return result;
        auto& target = *reinterpret_cast<thread*>(header);
        if (target.id == 0U || target.id >= user_thread_count)
            return error_t::denied;
        if (load_state(target) != state::terminated)
            return error_t::busy;
        task::task* owner = target.owner;
        if (owner == nullptr)
            return error_t::denied;

        for (u32 sibling = 0U; sibling < active_user_thread_count; ++sibling) {
            thread& other = user_threads[sibling];
            if (&other == &target || other.owner != owner ||
                other.object.type != object::type_t::thread)
                continue;
            result = quiesce_user_thread(other);
            if (result != error_t::success)
                return result;
            release_pending_reply(other);
            const object::reference_t other_scheduling =
                object::reference(other.scheduling_context.object);
            const object::reference_t other_space = object::reference(other.address_space.object);
            const object::reference_t other_thread = object::reference(other.object);
            {
                const capability::authority_guard authority_transaction{};
                store_state(other, state::terminated);
                memory::unmap_all(other.address_space);
                capability::revoke_reference_locked(other_thread);
                capability::revoke_reference_locked(other_space);
                capability::revoke_reference_locked(other_scheduling);
            }
            (void)object::unregister_object(other_scheduling);
            (void)object::unregister_object(other_space);
            (void)object::unregister_object(other_thread);
            other.address_space.release(&memory::release_physical_page);
            arch::thread::clear(other.context);
            other.owner = nullptr;
            other.object = {};
            other.address_space.object = {};
            other.scheduling_context.object = {};
            other.waiting_endpoint = 0U;
            other.reply = {};
            clear_transfer(other.transfer);
            other.ipc_timeout_active = false;
            other.last_fault = {};
            other.fault_disposition = fault::disposition::pending;
            store_state(other, state::inactive);
        }

        if (capability::slot_at(owner->cspace, 15U).object.type ==
            object::type_t::memory_resource) {
            result = memory::destroy_resource(*owner, 15U);
            if (result != error_t::success)
                return result;
        }

        const object::reference_t scheduling_reference =
            object::reference(target.scheduling_context.object);
        const object::reference_t space_reference = object::reference(target.address_space.object);
        const object::reference_t thread_reference = object::reference(target.object);
        const object::reference_t task_reference = object::reference(owner->object);
        {
            const capability::authority_guard authority_transaction{};
            store_state(target, state::terminated);
            memory::unmap_all(target.address_space);
            (void)capability::revoke_all_locked(
                owner->cspace, [](capability::derivation_id_t derivation) noexcept {
                    (void)memory::unmap_authority(derivation, false);
                });
            capability::revoke_reference_locked(thread_reference);
            capability::revoke_reference_locked(task_reference);
            capability::revoke_reference_locked(space_reference);
            capability::revoke_reference_locked(scheduling_reference);
            // Removes the CALLER's own capability at thread_selector, the
            // one thing destroy_user_bundle() does not need to do since
            // root's equivalent slots are covered by the revoke above --
            // but this selector lives in the caller's cspace, a different
            // cspace from owner->cspace, so the revoke above never reaches
            // it.
            (void)capability::delete_capability(current.owner->cspace, thread_selector);
        }
        memory::reclaim_task_memory(task_reference);
        result = object::unregister_object(scheduling_reference);
        if (result == error_t::success)
            result = object::unregister_object(space_reference);
        if (result == error_t::success)
            result = object::unregister_object(thread_reference);
        if (result == error_t::success)
            result = object::unregister_object(task_reference);
        if (result != error_t::success)
            return result;
        clear_user_bundle(target, *owner);
        return error_t::success;
    }

    inline void log_cpu_assignment(cpu_id_t cpu) noexcept {
        // Every thread could in principle pin to the same CPU, so this is
        // sized for the whole pool rather than an arbitrary small constant
        // -- see the comment below on what that constant used to cost.
        // Deliberately uninitialized: only indices [0, count) are ever
        // read below, and value-initializing an array this size lowers to
        // a memset call this freestanding kernel does not provide.
        thread_id_t assigned[user_thread_count];
        u32 count = 0U;
        for (u32 index = 0U; index < active_user_thread_count; ++index) {
            if (user_threads[index].pinned_cpu != cpu)
                continue;
            assigned[count] = user_threads[index].id;
            ++count;
        }

        /*
         * Built into one buffer and emitted with a single pr_info() call,
         * rather than one pr_info() per id: printk prepends its own
         * timestamp per call (CONFIG_PRINTK_TIME), so a sequence of calls
         * meant to form one logical line would instead interleave a
         * timestamp mid-line for every id past the first.
         *
         * Sized for user_thread_count ids at up to 20 digits (u64) plus a
         * comma each, which is generous -- thread ids are small counters --
         * but this is a diagnostic, not a wire format, and cheap to size
         * comfortably. Previously this array's declared size (assigned[3])
         * silently capped what a switch on a hardcoded 0/1/2/default ever
         * printed, backed by a static_assert that limited user_thread_count
         * itself to 3 per CPU -- a logging ceiling, not a scheduling one,
         * that would have quietly truncated output the moment a real
         * scheduling constraint allowed more.
         */
        // Also deliberately uninitialized past index `at`, for the same
        // reason: nothing reads past the NUL this function itself writes.
        char line[16U + user_thread_count * 21U];
        usize_t at = 0U;
        for (u32 index = 0U; index < count && at + 21U < sizeof(line); ++index) {
            if (index != 0U)
                line[at++] = ',';
            u64 value = assigned[index];
            char digits[20U];
            usize_t digit_count = 0U;
            do {
                digits[digit_count++] = static_cast<char>('0' + value % 10U);
                value /= 10U;
            } while (value != 0U);
            while (digit_count != 0U)
                line[at++] = digits[--digit_count];
        }
        line[at] = '\0';
        pr_info("user scheduler: cpu=%u threads=[%s]\n", static_cast<unsigned int>(cpu), line);
    }

    inline void log_pinning_table(u32 online_cpu_count) noexcept {
        pr_info("user scheduler pinning table:\n");
        const u32 count =
            online_cpu_count < maximum_cpu_count ? online_cpu_count : maximum_cpu_count;
        for (u32 cpu = 0U; cpu < count; ++cpu) {
            log_cpu_assignment(static_cast<cpu_id_t>(cpu));
        }
    }

    inline void launch_user_scheduler() noexcept {
        __atomic_store_n(&user_scheduler_ready, true, __ATOMIC_RELEASE);
        arch::cpu::wake_parked();
    }

    inline void wait_until_ready() noexcept {
        while (!__atomic_load_n(&user_scheduler_ready, __ATOMIC_ACQUIRE)) {
            arch::cpu::park();
        }
    }

    [[nodiscard]] inline u32 current_index() noexcept {
        return current_user_thread[arch::cpu::current_id()];
    }

    [[nodiscard]] inline thread& current() noexcept {
        return user_threads[current_index()];
    }

    inline void expire_ipc_timeouts(cpu_id_t cpu, u64 now) noexcept {
        for (;;) {
            const interrupt::timing::state irq_state = lock_timeout_queue(cpu);
            u32& count = timeout_queue_counts[cpu];
            if (count == 0U || timeout_queues[cpu][0].deadline > now) {
                unlock_timeout_queue(cpu, irq_state);
                return;
            }
            const timeout_entry entry = timeout_queues[cpu][0];
            for (u32 index = 1U; index < count; ++index)
                timeout_queues[cpu][index - 1U] = timeout_queues[cpu][index];
            --count;
            unlock_timeout_queue(cpu, irq_state);
            if (entry.thread >= active_user_thread_count)
                continue;
            thread& value = user_threads[entry.thread];
            if (value.object.generation != entry.generation || value.pinned_cpu != cpu ||
                !value.ipc_timeout_active || value.ipc_deadline != entry.deadline ||
                now < value.ipc_deadline)
                continue;
            const state current_state = load_state(value);
            if (current_state != state::blocked_send && current_state != state::blocked_receive &&
                current_state != state::blocked_reply && current_state != state::blocked_fault) {
                value.ipc_timeout_active = false;
                continue;
            }
            if (value.owner != nullptr && value.waiting_endpoint < capability::cspace_slot_count) {
                object::header_t* endpoint_header = nullptr;
                const capability::right_t endpoint_right = current_state == state::blocked_receive
                                                               ? capability::right_t::read
                                                               : capability::right_t::write;
                if (current_state != state::blocked_reply &&
                    capability::lookup(value.owner->cspace, value.waiting_endpoint,
                                       object::type_t::endpoint, endpoint_right,
                                       endpoint_header) == error_t::success &&
                    endpoint_header != nullptr) {
                    auto& endpoint = *reinterpret_cast<ipc::endpoint*>(endpoint_header);
                    (void)ipc::cancel_thread(endpoint, object::reference(value.object));
                }
            }
            if (!try_lock_ipc_lifecycle()) {
                arm_ipc_timeout(value);
                return;
            }
            if (load_state(value) != current_state || !value.ipc_timeout_active ||
                now < value.ipc_deadline) {
                unlock_ipc_lifecycle();
                continue;
            }
            for (u32 server_index = 0U; server_index < active_user_thread_count; ++server_index) {
                thread& server = user_threads[server_index];
                if (server.reply.valid && server.reply.caller == value.id &&
                    server.reply.generation == value.object.generation) {
                    if (server.reply.donation_active)
                        scheduling::revoke_donation(server.scheduling_context,
                                                    value.scheduling_context);
                    server.reply = {};
                }
            }
            value.ipc_timeout_active = false;
            clear_transfer(value.transfer);
            value.ipc_badge = 0U;
            value.pending_result = error_t::timed_out;
            if (current_state == state::blocked_fault) {
                value.fault_disposition = fault::disposition::terminate;
                value.waiting_endpoint = 0U;
                store_state(value, state::terminated);
            } else if (load_state(value) != state::faulted &&
                       load_state(value) != state::terminated) {
                store_state(value, state::ready);
            }
            unlock_ipc_lifecycle();
        }
    }

    [[nodiscard]] inline u64 next_timer_deadline(cpu_id_t cpu, u64 now) noexcept {
        if (cpu >= maximum_cpu_count || !user_execution_active[cpu] || !user_cpu_idle[cpu])
            return now == ~0ULL ? now : now + 1U;
        const interrupt::timing::state irq_state = lock_timeout_queue(cpu);
        const u64 idle_deadline = now <= ~0ULL - platform::timer::ticks_per_second
                                      ? now + platform::timer::ticks_per_second
                                      : ~0ULL;
        const u64 deadline =
            timeout_queue_counts[cpu] == 0U ? idle_deadline : timeout_queues[cpu][0].deadline;
        unlock_timeout_queue(cpu, irq_state);
        return deadline;
    }

    inline void save_current_user(const arch::thread::context& frame) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (user_execution_active[cpu]) {
            thread& value = user_threads[current_user_thread[cpu]];
            arch::thread::copy(value.context, frame);
            (void)scheduling::charge(value.scheduling_context, platform::timer::ticks(cpu), 1U);
        }
    }

    [[nodiscard]] inline u32 next_runnable(cpu_id_t cpu, u32 after) noexcept {
        const u64 now = platform::timer::ticks(cpu);
        u32 selected = after;
        u8 selected_priority = scheduling::lowest_priority;
        bool found = false;
        for (u32 offset = 1U; offset <= active_user_thread_count; ++offset) {
            const u32 candidate = (after + offset) % active_user_thread_count;
            thread& value = user_threads[candidate];
            if (value.pinned_cpu != cpu || !runnable(value) ||
                !scheduling::eligible(value.scheduling_context, now)) {
                continue;
            }
            const u8 priority = value.scheduling_context.effective_priority;
            if (!found || priority > selected_priority) {
                selected = candidate;
                selected_priority = priority;
                found = true;
            }
        }
        return found ? selected : after;
    }

    [[nodiscard]] inline bool validate_user_context(thread& value) noexcept {
        if (arch::thread::valid_user(value.context))
            return true;

        ++value.faults;
        store_state(value, state::faulted);
#if CONFIG_VERBOSE_DIAGNOSTICS
        pr_err("user context rejected thread=%llu cpu=%u pc=%llx sp=%llx status=%llx\n",
               static_cast<unsigned long long>(value.id),
               static_cast<unsigned int>(value.pinned_cpu),
               static_cast<unsigned long long>(value.context.instruction_pointer),
               static_cast<unsigned long long>(value.context.stack_pointer),
               static_cast<unsigned long long>(value.context.status));
#else
        pr_err("user context rejected thread=%llu cpu=%u\n",
               static_cast<unsigned long long>(value.id),
               static_cast<unsigned int>(value.pinned_cpu));
#endif
        return false;
    }

    inline void commit_kernel_idle(arch::thread::context& frame, thread* previous) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        arch::space::activate_kernel();
        arch::thread::prepare_kernel_idle(frame,
                                          reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
        user_cpu_idle[cpu] = true;
        __atomic_store_n(&current_user_generation[cpu], 0U, __ATOMIC_RELEASE);
        if (previous != nullptr)
            __atomic_store_n(&previous->executing, false, __ATOMIC_RELEASE);
    }

    inline void load_user(arch::thread::context& frame, u32 id) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        const u32 old_index = __atomic_load_n(&current_user_thread[cpu], __ATOMIC_ACQUIRE);
        const u32 old_generation = __atomic_load_n(&current_user_generation[cpu], __ATOMIC_ACQUIRE);
        thread& old = user_threads[old_index];
        const bool old_binding_valid =
            old.object.type == object::type_t::thread && old.object.generation == old_generation;
        if (old_binding_valid)
            (void)compare_state(old, state::running, state::ready);

        u32 candidate = id;
        for (u32 attempts = 0U; attempts < active_user_thread_count; ++attempts) {
            thread& value = user_threads[candidate];
            if (value.pinned_cpu == cpu && load_state(value) == state::ready &&
                validate_user_context(value)) {
                /*
                 * Publish the execution claim before claiming ready->running.
                 * Teardown may change ready to suspended concurrently; in that
                 * case the CAS fails and the execution claim is withdrawn.
                 * Once ready->running succeeds, teardown observes executing=true
                 * before it may reclaim the process bundle.
                 */
                __atomic_store_n(&value.executing, true, __ATOMIC_RELEASE);
                if (!compare_state(value, state::ready, state::running)) {
                    __atomic_store_n(&value.executing, false, __ATOMIC_RELEASE);
                    candidate = next_runnable(cpu, candidate);
                    continue;
                }
                __atomic_store_n(&current_user_thread[cpu], candidate, __ATOMIC_RELEASE);
                __atomic_store_n(&current_user_generation[cpu], value.object.generation,
                                 __ATOMIC_RELEASE);
                user_cpu_idle[cpu] = false;
                consume_pending(value);
                if (!validate_user_context(value)) {
                    __atomic_store_n(&value.executing, false, __ATOMIC_RELEASE);
                    store_state(value, state::faulted);
                    candidate = next_runnable(cpu, candidate);
                    continue;
                }
                value.address_space.activate();
                arch::thread::copy(frame, value.context);
                if (old_binding_valid &&
                    (candidate != old_index || value.object.generation != old_generation)) {
                    __atomic_store_n(&old.executing, false, __ATOMIC_RELEASE);
                }
                __atomic_fetch_add(&per_cpu_switches[cpu], 1U, __ATOMIC_RELAXED);
                emergency::trace(emergency::event::scheduler_switch, old_index, candidate,
                                 old_generation, value.object.generation);
                return;
            }
            candidate = next_runnable(cpu, candidate);
        }

        commit_kernel_idle(frame, old_binding_valid ? &old : nullptr);
    }

    inline void schedule_user(arch::thread::context& frame) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu])
            return;

        const u32 old = current_user_thread[cpu];
        expire_ipc_timeouts(cpu, platform::timer::ticks(cpu));
        save_current_user(frame);
        const u32 next = next_runnable(cpu, old);
        if (next != old || !runnable(user_threads[old]))
            load_user(frame, next);
    }

    [[nodiscard]] inline bool is_kernel_idle_frame(const arch::thread::context& frame) noexcept {
        return (frame.status & 0xfU) == 0x5U;
    }

    [[nodiscard]] inline bool resume_user_from_idle(arch::thread::context& frame) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu] || !user_cpu_idle[cpu] || !is_kernel_idle_frame(frame)) {
            return false;
        }

        expire_ipc_timeouts(cpu, platform::timer::ticks(cpu));
        const u32 old = current_user_thread[cpu];
        const u32 next = next_runnable(cpu, old);
        if (next != old || runnable(user_threads[next])) {
            load_user(frame, next);
            return !user_cpu_idle[cpu];
        }
        return false;
    }

    inline void prepare_block(arch::thread::context& frame, state blocked_state) noexcept {
        thread& value = current();
        arch::thread::copy(value.context, frame);

        store_state(value, blocked_state);
    }

    inline void schedule_prepared(arch::thread::context& frame) noexcept {
        thread& value = current();
        const u32 next = next_runnable(value.pinned_cpu, current_index());
        if (next == current_index() && !runnable(user_threads[next])) {
            commit_kernel_idle(frame, &value);
            return;
        }
        load_user(frame, next);
    }

    inline void block_and_schedule(arch::thread::context& frame, state blocked_state) noexcept {
        prepare_block(frame, blocked_state);
        schedule_prepared(frame);
    }

    [[nodiscard]] inline bool wake(thread& value) noexcept {
        /*
         * Wake only a thread that is still in a blocking state.  In
         * particular, never resurrect a suspended or terminated thread.
         * Teardown publishes suspended before waiting for remote quiescence;
         * a concurrent reply/cancel may have observed the earlier blocked
         * state, so the transition must be conditional rather than an
         * unconditional store to ready.
         */
        u8 observed =
            __atomic_load_n(reinterpret_cast<const u8*>(&value.current_state), __ATOMIC_ACQUIRE);
        for (;;) {
            const state current_state = static_cast<state>(observed);
            if (current_state != state::blocked_send && current_state != state::blocked_receive &&
                current_state != state::blocked_reply && current_state != state::blocked_fault) {
                return false;
            }
            const u8 desired = static_cast<u8>(state::ready);
            if (__atomic_compare_exchange_n(reinterpret_cast<u8*>(&value.current_state), &observed,
                                            desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                return true;
            }
        }
    }

    [[nodiscard]] inline bool deliver_fault_ipc(thread& value, arch::thread::context& frame,
                                                u64 syndrome, vaddr_t fault_address,
                                                fault::kind fault_kind) noexcept {
        if (value.owner == nullptr || value.owner->fault_endpoint == 0U)
            return false;
        if (value.last_fault.type != fault::kind::none &&
            value.fault_disposition == fault::disposition::pending)
            return false;
        capability::slot_t endpoint_slot{};
        const error_t lookup_result = capability::lookup_slot(
            value.owner->cspace, value.owner->fault_endpoint, object::type_t::endpoint,
            capability::right_t::write, endpoint_slot);
        object::header_t* endpoint_header = object::resolve(endpoint_slot.object);
        if (lookup_result != error_t::success || endpoint_header == nullptr)
            return false;
        value.ipc_badge = endpoint_slot.badge;

        value.last_fault.type = fault_kind;
        value.last_fault.thread = value.id;
        value.last_fault.thread_generation = value.object.generation;
        value.last_fault.syndrome = syndrome;
        value.last_fault.address = fault_address;
        value.last_fault.instruction_pointer = frame.instruction_pointer;
        value.fault_disposition = fault::disposition::pending;
        value.message[0] = static_cast<word_t>(value.last_fault.type);
        value.message[1] = static_cast<word_t>(syndrome);
        value.message[2] = static_cast<word_t>(fault_address);
        value.message[3] = static_cast<word_t>(frame.instruction_pointer);
        value.waiting_endpoint = value.owner->fault_endpoint;
        value.ipc_timeout_active = true;
        value.ipc_deadline = platform::timer::deadline_after(
            platform::timer::ticks(value.pinned_cpu), fault_timeout_ticks);
        arm_ipc_timeout(value);
        prepare_block(frame, state::blocked_fault);

        auto& endpoint = *reinterpret_cast<ipc::endpoint*>(endpoint_header);
        ipc::lock(endpoint);
        if (endpoint.receiver.type != object::type_t::none) {
            const object::reference_t receiver_reference = endpoint.receiver;
            endpoint.receiver = {};
            object::header_t* receiver_header = object::resolve(receiver_reference);
            if (receiver_header != nullptr && receiver_header->type == object::type_t::thread) {
                thread& receiver = *reinterpret_cast<thread*>(receiver_header);
                publish_pending(receiver, pending_ipc::incoming_call, value.id,
                                value.object.generation, value.ipc_badge, value.message);
                value.ipc_badge = 0U;
                (void)wake(receiver);
                ipc::remote_reschedule(receiver.pinned_cpu, arch::cpu::current_id());
                ipc::unlock(endpoint);
                schedule_prepared(frame);
                return true;
            }
        }
        if (!ipc::enqueue_sender(endpoint, object::reference(value.object))) {
            ipc::unlock(endpoint);
            store_state(value, state::faulted);
            return false;
        }
        ipc::unlock(endpoint);
        schedule_prepared(frame);
        return true;
    }

    [[nodiscard]] inline bool handle_user_fault(arch::thread::context& frame, u64 vector,
                                                u64 syndrome, vaddr_t fault_address) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu] || vector != 8U)
            return false;
        const u64 exception_class = (syndrome >> 26U) & 0x3fU;
        if (exception_class != 0x00U && exception_class != 0x20U && exception_class != 0x21U &&
            exception_class != 0x22U && exception_class != 0x24U && exception_class != 0x25U &&
            exception_class != 0x26U)
            return false;
        const fault::kind fault_kind = classify_user_fault(exception_class);
        const vaddr_t delivered_address =
            exception_class == 0x00U ? frame.instruction_pointer : fault_address;

        thread& value = current();
        ++value.faults;
        verification::mark_fault_ipc();
        emergency::trace(emergency::event::user_fault, value.id, syndrome, fault_address,
                         frame.instruction_pointer);
#if CONFIG_VERBOSE_DIAGNOSTICS
        pr_warn("user fault delivered thread=%llu cpu=%u esr=%llx far=%llx pc=%llx pager=%llu\n",
                static_cast<unsigned long long>(value.id), static_cast<unsigned int>(cpu),
                static_cast<unsigned long long>(syndrome),
                static_cast<unsigned long long>(fault_address),
                static_cast<unsigned long long>(frame.instruction_pointer),
                static_cast<unsigned long long>(value.owner != nullptr ? value.owner->fault_endpoint
                                                                       : 0U));
#else
        pr_warn("user fault delivered thread=%llu cpu=%u pager=%llu\n",
                static_cast<unsigned long long>(value.id), static_cast<unsigned int>(cpu),
                static_cast<unsigned long long>(value.owner != nullptr ? value.owner->fault_endpoint
                                                                       : 0U));
#endif
        if (deliver_fault_ipc(value, frame, syndrome, delivered_address, fault_kind))
            return true;
        store_state(value, state::faulted);
        const u32 next = next_runnable(cpu, current_index());
        if (next == current_index() && !runnable(user_threads[next])) {
            commit_kernel_idle(frame, &value);
            return true;
        }
        load_user(frame, next);
        return true;
    }

    [[noreturn]] inline void enter_first_user_thread() noexcept {
        if constexpr (!arch::thread::user_entry_ready)
            sys_kernel_user_idle();

        const cpu_id_t cpu = arch::cpu::current_id();
        arch::irq::disable();
        user_execution_active[cpu] = true;

        u32 first = cpu < active_user_thread_count ? cpu : 0U;
        if (first >= active_user_thread_count || user_threads[first].pinned_cpu != cpu ||
            !runnable(user_threads[first])) {
            first = next_runnable(cpu, first);
        }
        if (first >= active_user_thread_count || user_threads[first].pinned_cpu != cpu ||
            !runnable(user_threads[first])) {
            __atomic_store_n(&current_user_thread[cpu], 0U, __ATOMIC_RELEASE);
            __atomic_store_n(&current_user_generation[cpu], 0U, __ATOMIC_RELEASE);
            user_cpu_idle[cpu] = true;
            arch::irq::enable();
            sys_kernel_user_idle();
        }

        __atomic_store_n(&current_user_thread[cpu], first, __ATOMIC_RELEASE);
        __atomic_store_n(&current_user_generation[cpu], user_threads[first].object.generation,
                         __ATOMIC_RELEASE);
        user_cpu_idle[cpu] = false;
        __atomic_store_n(&user_threads[first].executing, true, __ATOMIC_RELEASE);
        if (!compare_state(user_threads[first], state::ready, state::running)) {
            __atomic_store_n(&user_threads[first].executing, false, __ATOMIC_RELEASE);
            user_cpu_idle[cpu] = true;
            arch::irq::enable();
            sys_kernel_user_idle();
        }
        consume_pending(user_threads[first]);
        user_threads[first].address_space.activate();
        arch::thread::enter_user(user_threads[first].context);
    }
} // namespace sys::kernel::thread
