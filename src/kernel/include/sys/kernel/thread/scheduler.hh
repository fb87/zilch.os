#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/entry.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/hypervisor.hh>
#include <sys/kernel/ipc/endpoint.hh>
#include <sys/kernel/verification/hooks.hh>
#if CONFIG_HYPERVISOR_SELFTEST
#include <sys/kernel/tests/hypervisor/control_models.hh>
#endif
#include <sys/kernel/boot/bootinfo.hh>
#include <sys/kernel/bootstrap.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/platform/timer.hh>
#include <sys/types.hh>

namespace sys::kernel::thread
{
    inline constexpr u32 user_thread_count = 10U;
    inline u32 active_user_thread_count = CONFIG_ROOT_ONLY_BOOT ? 1U : user_thread_count;
    inline constexpr u32 maximum_cpu_count = 4U;
    inline constexpr word_t memory_server_role = 0x100U;
    inline constexpr word_t fault_client_role = 0x101U;
    inline thread user_threads[user_thread_count]{};
    inline task::task user_tasks[user_thread_count]{};
    inline u32 current_user_thread[maximum_cpu_count]{};
    inline bool user_execution_active[maximum_cpu_count]{};
    inline bool user_cpu_idle[maximum_cpu_count]{};
    inline volatile bool user_scheduler_ready = false;
    extern "C" [[noreturn]] void sys_kernel_user_idle() noexcept;
    inline volatile u64 per_cpu_switches[maximum_cpu_count]{};

    [[nodiscard]] inline error_t initialize_user_threads() noexcept {
        error_t bootstrap_result = bootstrap::initialize_objects();
        if (bootstrap_result != error_t::success)
            return bootstrap_result;

        for (u32 index = 0U; index < ipc::endpoint_count; ++index) {
            ipc::initialize(ipc::endpoints[index]);
            const error_t result = object::register_object(ipc::endpoints[index].object,
                                                           static_cast<object_id_t>(32U + index),
                                                           object::type_t::endpoint);
            if (result != error_t::success)
                return result;
        }

        for (u32 id = 0U; id < active_user_thread_count; ++id) {
            task::initialize(user_tasks[id], static_cast<space_id_t>(id));
            error_t result = object::register_object(
                user_tasks[id].object, static_cast<object_id_t>(16U + id), object::type_t::task);
            if (result != error_t::success)
                return result;

            initialize_user(user_threads[id], static_cast<thread_id_t>(id),
                            static_cast<cpu_id_t>(id % maximum_cpu_count),
                            CONFIG_ROOT_ONLY_BOOT ? 0U : static_cast<word_t>(id),
                            CONFIG_ROOT_ONLY_BOOT ? 0U
                                                  : static_cast<word_t>(initial_fuzz_seed(id)));
            user_threads[id].owner = &user_tasks[id];
            user_tasks[id].fault_endpoint = 10U;
            result = object::register_object(user_threads[id].object, static_cast<object_id_t>(id),
                                             object::type_t::thread);
            if (result != error_t::success)
                return result;

            result = object::register_object(user_threads[id].address_space.object,
                                             static_cast<object_id_t>(60U + id),
                                             object::type_t::address_space);
            if (result != error_t::success)
                return result;
            result = object::register_object(user_threads[id].scheduling_context.object,
                                             static_cast<object_id_t>(50U + id),
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
        }
        task::task& root_task = user_tasks[0];
        const capability::rights_t root_memory_rights{
            static_cast<u32>(capability::right_t::read) |
            static_cast<u32>(capability::right_t::write) |
            static_cast<u32>(capability::right_t::grant) |
            static_cast<u32>(capability::right_t::control)};
        error_t root_result = capability::install(
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
        boot::root_bootinfo.capability_count = arch::hypervisor::active ? 10U : 8U;
        const capability_id_t selectors[10]{1U, 2U, 3U, 4U, 10U, 12U, 14U, 15U, 28U, 29U};
        for (u32 index = 0U; index < boot::root_bootinfo.capability_count; ++index) {
            const capability::slot_t& slot = root_task.cspace.slots[selectors[index]];
            boot::root_bootinfo.capabilities[index] = {selectors[index], slot.object,
                                                       slot.rights.bits};
        }

        for (u32 cpu = 0U; cpu < maximum_cpu_count; ++cpu) {
            current_user_thread[cpu] = CONFIG_ROOT_ONLY_BOOT ? 0U : cpu;
            user_execution_active[cpu] = false;
            user_cpu_idle[cpu] = false;
            per_cpu_switches[cpu] = 0U;
        }
        return error_t::success;
    }

    [[nodiscard]] inline error_t resolve_fault_with_frame(thread& pager, thread& target,
                                                          memory::frame& source, vaddr_t address,
                                                          memory::permission permissions) noexcept {
        if (load_state(target) != state::blocked_fault ||
            target.fault_disposition != fault::disposition::pending)
            return error_t::invalid_argument;
        if (target.owner == nullptr || pager.owner == nullptr)
            return error_t::denied;
        const vaddr_t page_address = address & ~(memory::page_size - 1U);
        const error_t result = memory::map(target.address_space, source, page_address, permissions);
        if (result != error_t::success)
            return result;
        target.fault_disposition = fault::disposition::resume;
        target.last_fault = {};
        target.waiting_endpoint = 0U;
        target.ipc_timeout_active = false;
        store_state(target, state::ready);
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
        target.owner = nullptr;
        target.object = {};
        target.address_space.object = {};
        target.scheduling_context.object = {};
        target.waiting_endpoint = 0U;
        target.reply = {};
        target.transfer = {};
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
                                                    capability_id_t space_selector) noexcept {
        if (!root.root || cpu >= maximum_cpu_count ||
            thread_selector >= capability::cspace_slot_count ||
            task_selector >= capability::cspace_slot_count ||
            space_selector >= capability::cspace_slot_count || thread_selector == task_selector ||
            thread_selector == space_selector || task_selector == space_selector) {
            return error_t::invalid_argument;
        }
        if (root.cspace.slots[thread_selector].object.type != object::type_t::none ||
            root.cspace.slots[task_selector].object.type != object::type_t::none ||
            root.cspace.slots[space_selector].object.type != object::type_t::none)
            return error_t::busy;

        const u32 id = find_free_user_slot(cpu);
        if (id >= user_thread_count)
            return error_t::no_memory;

        task::task& owner = user_tasks[id];
        thread& target = user_threads[id];
        task::initialize(owner, static_cast<space_id_t>(id));
        initialize_user(target, static_cast<thread_id_t>(id), cpu, role, initial_fuzz_seed(id));
        target.owner = &owner;
        owner.fault_endpoint = 10U;

        error_t result = object::register_dynamic_object(owner.object, object::type_t::task);
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
            result = capability::install(owner.cspace, 4U,
                                         object::reference(target.scheduling_context.object),
                                         control_rights);
        if (result == error_t::success) {
            capability::rights_t endpoint_rights = capability::rights(capability::right_t::write);
            if (role == memory_server_role) {
                endpoint_rights.bits |= static_cast<u32>(capability::right_t::read);
            }
            result = capability::install(
                owner.cspace, 10U, object::reference(ipc::endpoints[0].object), endpoint_rights);
        }
        if (result == error_t::success)
            result = capability::install(owner.cspace, 14U,
                                         object::reference(bootstrap::root_notification.object),
                                         capability::rights(capability::right_t::write));

        if (result != error_t::success) {
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
        certification_operations[id] = 0U;
        certification_failures[id] = 0U;
        return error_t::success;
    }

    [[nodiscard]] inline error_t quiesce_user_thread(thread& target) noexcept {
        store_state(target, state::suspended);
        platform::interrupt::send_ipi_all_others(platform::interrupt::reschedule_ipi);

        /*
         * State publication alone does not prove that a remote CPU has left
         * PL3.  Wait for the target CPU's exception/scheduler path to save
         * the context and publish executing=false before reclaiming or
         * replacing the address-space image.
         */
        constexpr u32 maximum_wait_rounds = 1000000U;
        for (u32 round = 0U; round < maximum_wait_rounds; ++round) {
            if (!__atomic_load_n(&target.executing, __ATOMIC_ACQUIRE))
                return error_t::success;
            if ((round & 0x3ffU) == 0U) {
                platform::interrupt::send_ipi_all_others(platform::interrupt::reschedule_ipi);
            }
            arch::cpu::relax();
        }
        return error_t::busy;
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
        if (target.id == 0U || target.id >= user_thread_count)
            return error_t::denied;
        result = quiesce_user_thread(target);
        if (result != error_t::success)
            return result;

        store_state(target, state::terminated);
        capability::revoke_reference(object::reference(target.object));
        capability::revoke_reference(object::reference(target.owner->object));
        capability::revoke_reference(object::reference(target.address_space.object));
        capability::revoke_reference(object::reference(target.scheduling_context.object));
        (void)capability::delete_capability(root.cspace, thread_selector);
        (void)capability::delete_capability(root.cspace, task_selector);
        (void)capability::delete_capability(root.cspace, space_selector);

        (void)object::unregister_object(object::reference(target.scheduling_context.object));
        (void)object::unregister_object(object::reference(target.address_space.object));
        task::task* owner = target.owner;
        (void)object::unregister_object(object::reference(target.object));
        (void)object::unregister_object(object::reference(owner->object));
        clear_user_bundle(target, *owner);
        return error_t::success;
    }

#if CONFIG_SELFTEST
    [[nodiscard]] inline error_t validate_bootstrap_objects() noexcept {
        task::task& root = user_tasks[0];
        error_t result = capability::copy(root.cspace, 16U, root.cspace, 10U,
                                          capability::rights(capability::right_t::write));
        if (result != error_t::success)
            return result;
        result = capability::move(root.cspace, 17U, root.cspace, 16U);
        if (result != error_t::success)
            return result;
        result = capability::delete_capability(root.cspace, 17U);
        if (result != error_t::success)
            return result;
        result = capability::mint(root.cspace, 18U, root.cspace, 10U,
                                  capability::rights(capability::right_t::write), 0x5aU);
        if (result != error_t::success)
            return result;
        const capability::derivation_id_t root_derivation = root.cspace.slots[10U].derivation;
        if (capability::revoke_descendants(root_derivation) != 1U ||
            root.cspace.slots[18U].object.type != object::type_t::none ||
            root.cspace.slots[10U].object.type == object::type_t::none)
            return error_t::invalid_argument;

        result = memory::map(
            user_threads[0].address_space, memory::frames[0],
            CONFIG_ROOT_ONLY_BOOT ? 0x20002000ULL : 0x10002000ULL,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)));
        if (result != error_t::success)
            return result;
        result = memory::unmap(user_threads[0].address_space, memory::frames[0]);
        if (result != error_t::success)
            return result;

        const paddr_t old_page = memory::frames[3].physical_address;
        auto* old_words = reinterpret_cast<volatile u64*>(static_cast<uintptr_t>(old_page));
        old_words[0] = 0xa5a55a5af00dcafeULL;
        result = memory::release_frame(memory::frames[3]);
        if (result != error_t::success)
            return result;
        result = memory::assign_frame(memory::frames[3], root.address_space_id);
        if (result != error_t::success || memory::frames[3].physical_address != old_page)
            return error_t::invalid_argument;
        const auto* new_words = reinterpret_cast<const volatile u64*>(
            static_cast<uintptr_t>(memory::frames[3].physical_address));
        if (new_words[0] != 0U)
            return error_t::invalid_argument;

        const u32 owned_before = root.memory_pages_owned;
        result = memory::create_frame(root, 18U);
        if (result != error_t::success || root.memory_pages_owned != owned_before + 1U)
            return error_t::invalid_argument;
        object::header_t* dynamic_header = nullptr;
        result = capability::lookup(root.cspace, 18U, object::type_t::frame,
                                    capability::right_t::control, dynamic_header);
        if (result != error_t::success || dynamic_header == nullptr)
            return error_t::invalid_argument;
        auto& dynamic_frame = *reinterpret_cast<memory::frame*>(dynamic_header);
        result = memory::map(
            user_threads[0].address_space, dynamic_frame,
            CONFIG_ROOT_ONLY_BOOT ? 0x20003000ULL : 0x10003000ULL,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)));
        if (result != error_t::success)
            return result;
        if (memory::destroy_frame(root, 18U) != error_t::busy)
            return error_t::invalid_argument;
        result = memory::unmap(user_threads[0].address_space, dynamic_frame);
        if (result != error_t::success)
            return result;
        result = memory::destroy_frame(root, 18U);
        if (result != error_t::success || root.memory_pages_owned != owned_before)
            return error_t::invalid_argument;

        result = memory::create_page_table(root, 19U, 3U);
        if (result != error_t::success || root.memory_pages_owned != owned_before + 1U)
            return error_t::invalid_argument;
        result = memory::destroy_page_table(root, 19U);
        if (result != error_t::success || root.memory_pages_owned != owned_before)
            return error_t::invalid_argument;
        const u32 pages_before_pager = root.memory_pages_owned;
        result = create_user_bundle(root, 1U, 0x50414745U, 20U, 21U, 22U);
        if (result != error_t::success)
            return result;
        object::header_t* pager_target_header = nullptr;
        result = capability::lookup(root.cspace, 20U, object::type_t::thread,
                                    capability::right_t::control, pager_target_header);
        if (result != error_t::success || pager_target_header == nullptr)
            return error_t::invalid_argument;
        auto& pager_target = *reinterpret_cast<thread*>(pager_target_header);
        result = memory::create_frame(root, 23U);
        if (result != error_t::success)
            return result;
        object::header_t* pager_frame_header = nullptr;
        result = capability::lookup(root.cspace, 23U, object::type_t::frame,
                                    capability::right_t::control, pager_frame_header);
        if (result != error_t::success || pager_frame_header == nullptr)
            return error_t::invalid_argument;
        auto& pager_frame = *reinterpret_cast<memory::frame*>(pager_frame_header);
        pager_target.last_fault = {fault::kind::data_abort,
                                   pager_target.id,
                                   pager_target.object.generation,
                                   0x96000004U,
                                   (arch::space::user_code + 0x4000ULL),
                                   pager_target.context.instruction_pointer};
        pager_target.fault_disposition = fault::disposition::pending;
        pager_target.waiting_endpoint = 10U;
        store_state(pager_target, state::blocked_fault);
        result = resolve_fault_with_frame(
            user_threads[0], pager_target, pager_frame, arch::space::user_code + 0x4000ULL,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)));
        if (result != error_t::success || load_state(pager_target) != state::ready ||
            pager_target.fault_disposition != fault::disposition::resume ||
            pager_frame.mapping_count != 1U)
            return error_t::invalid_argument;
        result = memory::unmap(pager_target.address_space, pager_frame,
                               arch::space::user_code + 0x4000ULL);
        if (result != error_t::success)
            return result;
        result = memory::destroy_frame(root, 23U);
        if (result != error_t::success)
            return result;
        store_state(pager_target, state::suspended);
        result = destroy_user_bundle(root, 20U, 21U, 22U);
        if (result != error_t::success || root.memory_pages_owned != pages_before_pager)
            return error_t::invalid_argument;
        pr_info("[TEST] name=pager_fault_reply result=PASS address=%llx\n",
                static_cast<unsigned long long>(arch::space::user_code + 0x4000ULL));

        pr_info("[TEST] name=dynamic_memory_objects result=PASS free=%u managed=%u\n",
                static_cast<unsigned int>(memory::free_pages),
                static_cast<unsigned int>(memory::managed_pages));

        notification::signal(bootstrap::root_notification, 1U);
        if (notification::consume(bootstrap::root_notification) != 1U)
            return error_t::invalid_argument;
        verification::mark_bootstrap_self_tests(true, true, true);
        if constexpr (arch::hypervisor::active) {
            result = hypervisor::test::run_all();
            if (result != error_t::success)
                return result;
            pr_info("[HV-DIAG] suite=single-vcpu self-test=PASS operations=%llu failures=%llu\n",
                    static_cast<unsigned long long>(hypervisor::test::operations),
                    static_cast<unsigned long long>(hypervisor::test::failures_total));
        }
        return error_t::success;
    }
#endif

    inline void log_cpu_assignment(cpu_id_t cpu) noexcept {
        thread_id_t assigned[3]{};
        u32 count = 0U;
        for (u32 index = 0U; index < active_user_thread_count; ++index) {
            if (user_threads[index].pinned_cpu != cpu)
                continue;
            if (count < 3U)
                assigned[count] = user_threads[index].id;
            ++count;
        }

        switch (count) {
            case 0U:
                pr_info("user scheduler: cpu=%u threads=[]\n", static_cast<unsigned int>(cpu));
                break;
            case 1U:
                pr_info("user scheduler: cpu=%u threads=[%llu]\n", static_cast<unsigned int>(cpu),
                        static_cast<unsigned long long>(assigned[0]));
                break;
            case 2U:
                pr_info("user scheduler: cpu=%u threads=[%llu,%llu]\n",
                        static_cast<unsigned int>(cpu),
                        static_cast<unsigned long long>(assigned[0]),
                        static_cast<unsigned long long>(assigned[1]));
                break;
            default:
                pr_info("user scheduler: cpu=%u threads=[%llu,%llu,%llu]\n",
                        static_cast<unsigned int>(cpu),
                        static_cast<unsigned long long>(assigned[0]),
                        static_cast<unsigned long long>(assigned[1]),
                        static_cast<unsigned long long>(assigned[2]));
                break;
        }
    }

    inline void log_pinning_table(u32 online_cpu_count) noexcept {
        static_assert(((user_thread_count + maximum_cpu_count - 1U) / maximum_cpu_count) <= 3U);
        pr_info("user scheduler pinning table:\n");
        const u32 count =
            online_cpu_count < maximum_cpu_count ? online_cpu_count : maximum_cpu_count;
        for (u32 cpu = 0U; cpu < count; ++cpu) {
            log_cpu_assignment(static_cast<cpu_id_t>(cpu));
        }
    }

    inline void launch_user_scheduler() noexcept {
        __atomic_store_n(&user_scheduler_ready, true, __ATOMIC_RELEASE);
        __asm__ volatile("sev" ::: "memory");
    }

    inline void wait_until_ready() noexcept {
        while (!__atomic_load_n(&user_scheduler_ready, __ATOMIC_ACQUIRE)) {
            __asm__ volatile("wfe" ::: "memory");
        }
    }

    [[nodiscard]] inline u32 current_index() noexcept {
        return current_user_thread[arch::cpu::current_id()];
    }

    [[nodiscard]] inline thread& current() noexcept {
        return user_threads[current_index()];
    }

    inline void expire_ipc_timeouts(cpu_id_t cpu, u64 now) noexcept {
        for (u32 index = 0U; index < active_user_thread_count; ++index) {
            thread& value = user_threads[index];
            if (value.pinned_cpu != cpu || !value.ipc_timeout_active || now < value.ipc_deadline)
                continue;
            const state current_state = load_state(value);
            if (current_state != state::blocked_send && current_state != state::blocked_receive &&
                current_state != state::blocked_reply) {
                value.ipc_timeout_active = false;
                continue;
            }
            if (value.owner != nullptr && value.waiting_endpoint < capability::cspace_slot_count) {
                object::header_t* endpoint_header = nullptr;
                if (capability::lookup(value.owner->cspace, value.waiting_endpoint,
                                       object::type_t::endpoint, capability::right_t::read,
                                       endpoint_header) == error_t::success &&
                    endpoint_header != nullptr) {
                    auto& endpoint = *reinterpret_cast<ipc::endpoint*>(endpoint_header);
                    (void)ipc::cancel_thread(endpoint, object::reference(value.object));
                }
            }
            for (u32 server_index = 0U; server_index < active_user_thread_count; ++server_index) {
                thread& server = user_threads[server_index];
                if (server.reply.valid && server.reply.caller == value.id &&
                    server.reply.generation == value.object.generation) {
                    if (server.reply.donation_active)
                        scheduling::revoke_donation(server.scheduling_context);
                    server.reply = {};
                }
            }
            value.ipc_timeout_active = false;
            value.transfer = {};
            value.pending_result = error_t::timed_out;
            if (load_state(value) != state::faulted && load_state(value) != state::terminated)
                store_state(value, state::ready);
        }
    }

    inline void save_current_user(const arch::thread::context& frame) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (user_execution_active[cpu]) {
            thread& value = user_threads[current_user_thread[cpu]];
            arch::thread::copy(value.context, frame);
            __atomic_store_n(&value.executing, false, __ATOMIC_RELEASE);
            (void)scheduling::charge(value.scheduling_context, 1U);
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
        pr_err("user context rejected thread=%llu cpu=%u pc=%llx sp=%llx status=%llx\n",
               static_cast<unsigned long long>(value.id),
               static_cast<unsigned int>(value.pinned_cpu),
               static_cast<unsigned long long>(value.context.instruction_pointer),
               static_cast<unsigned long long>(value.context.stack_pointer),
               static_cast<unsigned long long>(value.context.status));
        return false;
    }

    inline void load_user(arch::thread::context& frame, u32 id) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        thread& old = user_threads[current_user_thread[cpu]];
        if (load_state(old) == state::running)
            store_state(old, state::ready);

        u32 candidate = id;
        for (u32 attempts = 0U; attempts < active_user_thread_count; ++attempts) {
            thread& value = user_threads[candidate];
            if (value.pinned_cpu == cpu && runnable(value) && validate_user_context(value)) {
                current_user_thread[cpu] = candidate;
                user_cpu_idle[cpu] = false;
                store_state(value, state::running);
                consume_pending(value);
                if (!validate_user_context(value)) {
                    candidate = next_runnable(cpu, candidate);
                    continue;
                }
                value.address_space.activate();
                __atomic_store_n(&value.executing, true, __ATOMIC_RELEASE);
                arch::thread::copy(frame, value.context);
                __atomic_fetch_add(&per_cpu_switches[cpu], 1U, __ATOMIC_RELAXED);
                return;
            }
            candidate = next_runnable(cpu, candidate);
        }

        user_cpu_idle[cpu] = true;
        arch::thread::prepare_kernel_idle(frame,
                                          reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
    }

    inline void schedule_user(arch::thread::context& frame) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu])
            return;

        /* This path is valid only when the IRQ interrupted user mode. */
        const u32 old = current_user_thread[cpu];
        expire_ipc_timeouts(cpu, platform::timer::ticks(cpu));
        save_current_user(frame);
        const u32 next = next_runnable(cpu, old);
        if (next != old || !runnable(user_threads[old]))
            load_user(frame, next);
    }

    [[nodiscard]] inline bool is_kernel_idle_frame(const arch::thread::context& frame) noexcept {
        /*
         * An IRQ interrupts at an instruction inside sys_kernel_user_idle(),
         * normally the architecture wait-for-event instruction, rather than
         * at the function entry address.  The per-CPU user_cpu_idle flag is
         * published only when the scheduler deliberately enters this EL1h
         * idle context and is cleared before installing an EL0 context.
         * Combined with the lower-EL IRQ vector check in arch.cc, EL1h is the
         * correct architectural provenance test here.
         */
        return (frame.status & 0xfU) == 0x5U;
    }

    [[nodiscard]] inline bool resume_user_from_idle(arch::thread::context& frame) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!user_execution_active[cpu] || !user_cpu_idle[cpu] || !is_kernel_idle_frame(frame)) {
            return false;
        }

        /*
         * This is the only EL1 exception frame that may be replaced by an
         * EL0 context.  IRQs interrupting syscall, fault, printk, scheduler,
         * or other kernel execution must return to that kernel instruction.
         */
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

        /*
         * A thread becomes reclaimable only after the kernel has committed to
         * not returning this exception frame to it.  Publishing quiescence on
         * generic EL0 exception entry is unsafe because another CPU may then
         * destroy the address space while the syscall or fault handler still
         * owns the context.  Blocking is an explicit hand-off point: the
         * saved context is complete and schedule_prepared() will install a
         * different user context or the kernel-idle frame.
         */
        __atomic_store_n(&value.executing, false, __ATOMIC_RELEASE);
        store_state(value, blocked_state);
    }

    inline void schedule_prepared(arch::thread::context& frame) noexcept {
        thread& value = current();
        const u32 next = next_runnable(value.pinned_cpu, current_index());
        if (next == current_index() && !runnable(user_threads[next])) {
            user_cpu_idle[value.pinned_cpu] = true;
            arch::thread::prepare_kernel_idle(frame,
                                              reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
            return;
        }
        load_user(frame, next);
    }

    inline void block_and_schedule(arch::thread::context& frame, state blocked_state) noexcept {
        prepare_block(frame, blocked_state);
        schedule_prepared(frame);
    }

    inline void wake(thread& value) noexcept {
        if (value.current_state != state::faulted && value.current_state != state::terminated) {
            store_state(value, state::ready);
        }
    }

    [[nodiscard]] inline bool deliver_fault_ipc(thread& value, arch::thread::context& frame,
                                                u64 syndrome, vaddr_t fault_address) noexcept {
        if (value.owner == nullptr || value.owner->fault_endpoint == 0U)
            return false;
        object::header_t* endpoint_header = nullptr;
        const error_t lookup_result = capability::lookup(
            value.owner->cspace, value.owner->fault_endpoint, object::type_t::endpoint,
            capability::right_t::write, endpoint_header);
        if (lookup_result != error_t::success || endpoint_header == nullptr)
            return false;

        value.last_fault.type = fault::kind::data_abort;
        value.last_fault.thread = value.id;
        value.last_fault.thread_generation = value.object.generation;
        value.last_fault.syndrome = syndrome;
        value.last_fault.address = fault_address;
        value.last_fault.instruction_pointer = frame.instruction_pointer;
        value.fault_disposition = fault::disposition::pending;
        value.message[0] = static_cast<word_t>(fault::disposition::terminate);
        value.message[1] = static_cast<word_t>(value.last_fault.type);
        value.message[2] = static_cast<word_t>(fault_address);
        value.message[3] = static_cast<word_t>(frame.instruction_pointer);
        value.waiting_endpoint = value.owner->fault_endpoint;
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
                                value.object.generation, value.message);
                wake(receiver);
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
        if (exception_class != 0x20U && exception_class != 0x21U && exception_class != 0x22U &&
            exception_class != 0x24U && exception_class != 0x25U && exception_class != 0x26U)
            return false;

        thread& value = current();
        ++value.faults;
        verification::mark_fault_ipc();
        pr_warn("user fault delivered thread=%llu cpu=%u esr=%llx far=%llx pc=%llx pager=%llu\n",
                static_cast<unsigned long long>(value.id), static_cast<unsigned int>(cpu),
                static_cast<unsigned long long>(syndrome),
                static_cast<unsigned long long>(fault_address),
                static_cast<unsigned long long>(frame.instruction_pointer),
                static_cast<unsigned long long>(value.owner != nullptr ? value.owner->fault_endpoint
                                                                       : 0U));
        if (deliver_fault_ipc(value, frame, syndrome, fault_address))
            return true;
        store_state(value, state::faulted);
        const u32 next = next_runnable(cpu, current_index());
        if (next == current_index() && !runnable(user_threads[next])) {
            /*
             * The user fault has already been contained.  It is valid for the
             * remaining threads pinned to this CPU to be blocked in IPC.  In
             * that case, return to the per-CPU EL1 idle context and wait for a
             * timer or remote reschedule IPI to make a thread runnable.
             */
            user_cpu_idle[cpu] = true;
            arch::thread::prepare_kernel_idle(frame,
                                              reinterpret_cast<uintptr_t>(&sys_kernel_user_idle));
            return true;
        }
        load_user(frame, next);
        return true;
    }

    [[noreturn]] inline void enter_first_user_thread() noexcept {
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
            current_user_thread[cpu] = 0U;
            user_cpu_idle[cpu] = true;
            arch::irq::enable();
            sys_kernel_user_idle();
        }

        current_user_thread[cpu] = first;
        user_cpu_idle[cpu] = false;
        store_state(user_threads[first], state::running);
        consume_pending(user_threads[first]);
        user_threads[first].address_space.activate();
        __atomic_store_n(&user_threads[first].executing, true, __ATOMIC_RELEASE);
        arch::thread::enter_user(user_threads[first].context);
    }
} // namespace sys::kernel::thread
