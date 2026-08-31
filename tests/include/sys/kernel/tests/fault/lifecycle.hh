#pragma once

/*
 * NOT a standalone-includable header: this file is #include'd from a
 * specific point inside sys::kernel::thread::scheduler.hh's own body (see
 * that file's validate_bootstrap_objects()), after user_threads[],
 * store_state(), resolve_fault_with_frame(), create_user_bundle()/
 * destroy_user_bundle(), arm_ipc_timeout()/expire_ipc_timeouts(), and
 * timeout_queue_counts/timeout_queues have already been declared there --
 * all sys::kernel::thread-internal symbols with no home outside that file.
 * Including this anywhere else, or before that point in scheduler.hh, will
 * fail to compile. This mirrors elf64_dynamic_check.hh's approach to the
 * same kind of unavoidable coupling to its host file's internals.
 */
namespace sys::kernel::tests::fault_lifecycle
{
    [[nodiscard]] inline error_t run(task::task& root) noexcept {
        constexpr vaddr_t scratch_mapping_address = arch::space::user_code + 0x8000ULL;
        constexpr vaddr_t dynamic_mapping_address = arch::space::user_code + 0x9000ULL;
        static_assert(dynamic_mapping_address < arch::space::user_stack_base);

        error_t result = memory::map(
            thread::user_threads[0].address_space, memory::frames[0], scratch_mapping_address,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)),
            0U, 0U);
        if (result != error_t::success)
            return result;
        result = memory::unmap(thread::user_threads[0].address_space, memory::frames[0]);
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
            thread::user_threads[0].address_space, dynamic_frame, dynamic_mapping_address,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)),
            0U, 0U);
        if (result != error_t::success)
            return result;
        if (memory::destroy_frame(root, 18U) != error_t::busy)
            return error_t::invalid_argument;
        result = memory::unmap(thread::user_threads[0].address_space, dynamic_frame);
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
        result = thread::create_user_bundle(root, 1U, thread::fault_client_role, 20U, 21U, 22U);
        if (result != error_t::success)
            return result;
        object::header_t* pager_target_header = nullptr;
        result = capability::lookup(root.cspace, 20U, object::type_t::thread,
                                    capability::right_t::control, pager_target_header);
        if (result != error_t::success || pager_target_header == nullptr)
            return error_t::invalid_argument;
        auto& pager_target = *reinterpret_cast<thread::thread*>(pager_target_header);
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
                                   0x96000044U,
                                   (arch::space::user_code + 0x4000ULL),
                                   pager_target.context.instruction_pointer};
        pager_target.fault_disposition = fault::disposition::pending;
        pager_target.waiting_endpoint = 10U;
        thread::store_state(pager_target, thread::state::blocked_fault);
        result = thread::resolve_fault_with_frame(
            thread::user_threads[0], pager_target, pager_frame, arch::space::user_code + 0x5000ULL,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)));
        if (result != error_t::invalid_argument ||
            thread::load_state(pager_target) != thread::state::blocked_fault ||
            pager_target.fault_disposition != fault::disposition::pending ||
            pager_frame.mapping_count != 0U)
            return error_t::invalid_argument;
        result = thread::resolve_fault_with_frame(thread::user_threads[0], pager_target,
                                                  pager_frame, arch::space::user_code + 0x4000ULL,
                                                  memory::permission::read);
        if (result != error_t::denied ||
            thread::load_state(pager_target) != thread::state::blocked_fault ||
            pager_target.fault_disposition != fault::disposition::pending ||
            pager_frame.mapping_count != 0U)
            return error_t::invalid_argument;
        result = thread::resolve_fault_with_frame(
            thread::user_threads[0], pager_target, pager_frame, arch::space::user_code + 0x4000ULL,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)));
        if (result != error_t::success ||
            thread::load_state(pager_target) != thread::state::ready ||
            pager_target.fault_disposition != fault::disposition::resume ||
            pager_frame.mapping_count != 1U)
            return error_t::invalid_argument;
        pager_target.last_fault = {fault::kind::data_abort,
                                   pager_target.id,
                                   pager_target.object.generation,
                                   0x96000004U,
                                   (arch::space::user_code + 0x4000ULL),
                                   pager_target.context.instruction_pointer};
        pager_target.fault_disposition = fault::disposition::pending;
        pager_target.waiting_endpoint = 10U;
        thread::store_state(pager_target, thread::state::blocked_fault);
        result = thread::resolve_fault_with_frame(
            thread::user_threads[0], pager_target, pager_frame, arch::space::user_code + 0x4000ULL,
            static_cast<memory::permission>(static_cast<u8>(memory::permission::read) |
                                            static_cast<u8>(memory::permission::write)));
        if (result != error_t::success ||
            thread::load_state(pager_target) != thread::state::ready ||
            pager_frame.mapping_count != 1U)
            return error_t::invalid_argument;
        pager_target.last_fault = {fault::kind::data_abort,
                                   pager_target.id,
                                   pager_target.object.generation,
                                   0x96000004U,
                                   (arch::space::user_code + 0x5000ULL),
                                   pager_target.context.instruction_pointer};
        pager_target.fault_disposition = fault::disposition::pending;
        if (thread::deliver_fault_ipc(pager_target, pager_target.context, 0x96000004U,
                                      arch::space::user_code + 0x5000ULL,
                                      fault::kind::data_abort) ||
            thread::load_state(pager_target) != thread::state::ready)
            return error_t::invalid_argument;
        pager_target.last_fault = {fault::kind::data_abort,
                                   pager_target.id,
                                   pager_target.object.generation,
                                   0x96000004U,
                                   (arch::space::user_code + 0x6000ULL),
                                   pager_target.context.instruction_pointer};
        pager_target.fault_disposition = fault::disposition::pending;
        pager_target.waiting_endpoint = 10U;
        pager_target.ipc_timeout_active = true;
        pager_target.ipc_deadline = 1U;
        thread::store_state(pager_target, thread::state::blocked_fault);
        thread::arm_ipc_timeout(pager_target);
        if (thread::timeout_queue_counts[pager_target.pinned_cpu] != 1U ||
            thread::timeout_queues[pager_target.pinned_cpu][0].deadline != 1U)
            return error_t::invalid_argument;
        thread::expire_ipc_timeouts(pager_target.pinned_cpu, 2U);
        if (thread::load_state(pager_target) != thread::state::terminated ||
            pager_target.fault_disposition != fault::disposition::terminate ||
            pager_target.ipc_timeout_active)
            return error_t::invalid_argument;
        pr_info("[TEST] name=same_page_fault_serialization result=PASS mappings=1\n");
        pr_info("[TEST] name=pager_invalid_reply result=PASS wrong_page=reject write_ro=reject\n");
        pr_info("[TEST] name=nested_fault_bound result=PASS depth=1\n");
        pr_info("[TEST] name=pager_timeout_death result=PASS disposition=terminate\n");
        pr_info("[TEST] name=timeout_queue_order result=PASS expired=1 remaining=0\n");
        result = memory::unmap(pager_target.address_space, pager_frame,
                               arch::space::user_code + 0x4000ULL);
        if (result != error_t::success)
            return result;
        result = memory::destroy_frame(root, 23U);
        if (result != error_t::success)
            return result;
        thread::store_state(pager_target, thread::state::suspended);
        result = thread::destroy_user_bundle(root, 20U, 21U, 22U);
        if (result != error_t::success || root.memory_pages_owned != pages_before_pager)
            return error_t::invalid_argument;
        pr_info("[TEST] name=pager_fault_reply result=PASS address=%llx\n",
                static_cast<unsigned long long>(arch::space::user_code + 0x4000ULL));

        pr_info("[TEST] name=dynamic_memory_objects result=PASS free=%u managed=%u\n",
                static_cast<unsigned int>(memory::free_pages),
                static_cast<unsigned int>(memory::managed_pages));
        if (!memory::mapping_database_valid())
            return error_t::invalid_argument;
        return error_t::success;
    }
} // namespace sys::kernel::tests::fault_lifecycle
