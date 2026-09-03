#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/memory.hh>
#include <sys/arch/space/asid.hh>
#include <sys/arch/space/elf64.hh>
#include <sys/platform/v1/earlyfs.hh>
#include <sys/types.hh>

namespace sys::arch::space
{
    inline constexpr bool user_available = true;
#if CONFIG_ROOT_ONLY_BOOT
    inline constexpr vaddr_t user_code = 0x20000000ULL;
#else
    inline constexpr vaddr_t user_code = 0x10000000ULL;
#endif

    /*
     * The user virtual layout, in one place.
     *
     * Every process used to get exactly the image window plus a single
     * stack page, because the entire address space was one statically
     * embedded L3 table covering one 2 MiB block and map_page() rejected
     * every address outside it. That is enough for a server that never
     * allocates; it is not enough for a program with a heap, so the window
     * now spans several L2 blocks whose L3 tables are allocated on demand.
     *
     *   [user_code, +bootstrap_size)       ELF image, 63 loadable pages
     *   the page below user_stack_base     permanent unmapped guard
     *   [user_stack_base, +stack_size)     stack, grows down onto the guard
     *   [user_heap_base, user_window_end)  heap and everything else
     *
     * The guard is not new -- elf64::stack_guard_pages already reserved the
     * image window's top page -- but it matters more now that the stack is
     * multi-page and something can actually run far enough to overflow it.
     */
    inline constexpr vaddr_t user_stack_base = user_code + elf64::bootstrap_size;
    inline constexpr usize_t user_stack_pages = 16U;
    inline constexpr usize_t user_stack_size = user_stack_pages * memory::page_size;
    inline constexpr vaddr_t user_heap_base = user_stack_base + user_stack_size;

    /* One L2 block is 2 MiB; the window is a whole number of them so a
     * virtual address maps to an L3 table by a shift, with no partial
     * block at either end. */
    inline constexpr u64 user_block_size = 0x200000ULL;
    inline constexpr usize_t user_block_count = 16U;
    inline constexpr vaddr_t user_window_base = user_code;
    inline constexpr vaddr_t user_window_end =
        user_window_base + user_block_count * user_block_size;

    // TTBR0 currently preserves the kernel identity block at 0x40000000.
    // User mappings must remain below that L1 block until kernel mappings
    // move to TTBR1_EL1.
    inline constexpr vaddr_t kernel_identity_base = 0x40000000ULL;
    static_assert((user_code & (user_block_size - 1U)) == 0U,
                  "user_code must be L2-block aligned: the window indexes L3 tables from it");
    static_assert(user_code < kernel_identity_base);
    static_assert(user_window_end <= kernel_identity_base);
    static_assert(user_heap_base < user_window_end);
    static_assert(user_stack_base + user_stack_size <= user_window_base + user_block_size,
                  "image, guard and stack must share block 0's statically embedded L3");

#if CONFIG_ROOT_ONLY_BOOT
    extern "C" char sys_arm64_earlyfs_image_start[];
    extern "C" char sys_arm64_earlyfs_image_end[];
#else
    extern "C" char sys_arm64_user_image_start[];
    extern "C" char sys_arm64_user_image_end[];
#endif

    inline constexpr word_t memory_server_image_role = 0x100U;
    inline constexpr word_t pager_client_image_role_base = 0x101U;
    inline constexpr word_t memory_client_image_role_base = 0x103U;
    inline constexpr word_t undefined_instruction_image_role = 0x106U;
    inline constexpr word_t ipc_lifecycle_client_role_base = 0x110U;
    inline constexpr word_t control_plane_image_role_base = 0x200U;
    inline constexpr word_t control_plane_image_role_count = 5U;
    inline constexpr word_t domain_manager_image_role = 0x203U;
    inline constexpr word_t console_server_image_role = 0x202U;
    /*
     * console-server spawns a second thread of its own (via thread_create,
     * not process_create -- see src/user/servers/console/main.cc's
     * stdin_main()) under this role, unconditionally, in every environment
     * that launches console-server's binary at all -- not just under
     * root_graph.hh's production boot (which binds it explicitly via
     * bind_role_image()), but also the legacy CONFIG_SELFTEST harness in
     * this same file's caller (init/main.cc's create_console_service_
     * process()), which has no root_graph.hh-style dynamic binding at all.
     * Without this static fallback, resolution would silently fall through
     * to the generic "bin/init" default below and the new thread would run
     * init's own code instead of console-server's -- confirmed causing a
     * real hang (that thread falls into init's generic worker() infinite
     * loop, permanently pinned to a CPU) the first time this was tested
     * under the legacy harness.
     */
    inline constexpr word_t console_stdin_image_role = 0x108U;

    struct image_view {
        char* start;
        char* end;
    };

    /*
     * Every PL3 server binary lives as one named entry in a single earlyfs
     * image instead of being individually .incbin'd -- see
     * include/sys/platform/v1/earlyfs.hh. This function still maps a role
     * word onto a name (that policy hasn't moved to userspace yet -- see the
     * project roadmap's "remove role-to-image policy from the architecture
     * backend" item, deliberately out of scope for this pass), but the
     * bytes themselves come from one shared image looked up by path.
     */
    /*
     * Root-driven path lookup, phase 1: lets a root-privileged task bind a
     * role to a (offset, size) span within the linked-in earlyfs image that
     * IT resolved by name (via a mapped read-only earlyfs_page_address()
     * frame and platform::v1::earlyfs::find(), both in userspace) instead
     * of the kernel/arch layer hardcoding role->name policy. Bounds are
     * re-validated here against the kernel's own known image size
     * regardless of what the caller claims -- a bound bad offset/size is
     * simply rejected, it can never cause an out-of-bounds read later,
     * since image_for_role() only ever returns spans that passed this
     * check.
     *
     * Deliberately additive and inert until used: image_for_role() below
     * consults this table first and only falls back to the historical
     * hardcoded switch for roles nobody has bound, so real boot behavior is
     * unchanged unless/until root_graph.hh is updated to actually call
     * this. That cutover -- and removing the hardcoded switch entirely --
     * is a later, separate step, mirroring how elf64::load_dynamic() was
     * introduced and proven before address_space::initialize() was cut
     * over to it.
     */
    inline constexpr word_t maximum_role_bindings = 16U;
    struct role_binding {
        word_t role{};
        u64 offset{};
        u64 size{};
        bool used{};
    };
    inline role_binding role_bindings[maximum_role_bindings]{};

#if CONFIG_ROOT_ONLY_BOOT
    [[nodiscard]] inline usize_t earlyfs_image_size() noexcept {
        return static_cast<usize_t>(sys_arm64_earlyfs_image_end - sys_arm64_earlyfs_image_start);
    }

    [[nodiscard]] inline bool earlyfs_page_address(word_t page_index, paddr_t& address) noexcept {
        const usize_t page_count =
            (earlyfs_image_size() + memory::page_size - 1U) / memory::page_size;
        if (page_index >= page_count)
            return false;
        address = reinterpret_cast<paddr_t>(sys_arm64_earlyfs_image_start) +
                 static_cast<paddr_t>(page_index) * memory::page_size;
        return true;
    }
#else
    [[nodiscard]] inline usize_t earlyfs_image_size() noexcept {
        return 0U;
    }

    [[nodiscard]] inline bool earlyfs_page_address(word_t, paddr_t&) noexcept {
        return false;
    }
#endif

    [[nodiscard]] inline error_t bind_role_image(word_t role, u64 offset, u64 size) noexcept {
        const auto image_size = static_cast<u64>(earlyfs_image_size());
        if (size == 0U || offset > image_size || size > image_size - offset)
            return error_t::invalid_argument;
        role_binding* slot = nullptr;
        for (auto& entry : role_bindings) {
            if (entry.used && entry.role == role) {
                slot = &entry;
                break;
            }
            if (slot == nullptr && !entry.used)
                slot = &entry;
        }
        if (slot == nullptr)
            return error_t::no_memory;
        slot->role = role;
        slot->offset = offset;
        slot->size = size;
        slot->used = true;
        return error_t::success;
    }

    [[nodiscard]] inline bool find_role_binding(word_t role, u64& offset, u64& size) noexcept {
        for (const auto& entry : role_bindings) {
            if (entry.used && entry.role == role) {
                offset = entry.offset;
                size = entry.size;
                return true;
            }
        }
        return false;
    }

#if CONFIG_ROOT_ONLY_BOOT
    [[nodiscard]] inline image_view image_for_role(word_t role) noexcept {
        u64 bound_offset = 0U;
        u64 bound_size = 0U;
        if (find_role_binding(role, bound_offset, bound_size)) {
            auto* start = sys_arm64_earlyfs_image_start + bound_offset;
            return {start, start + bound_size};
        }

        const char* name = "bin/init";
        if (role == memory_server_image_role)
            name = "bin/memory-server";
        else if (role == console_stdin_image_role)
            name = "bin/console-server";
#if CONFIG_TESTS
        else if (role == pager_client_image_role_base || role == pager_client_image_role_base + 1U ||
                 role == undefined_instruction_image_role ||
                 role == ipc_lifecycle_client_role_base ||
                 role == ipc_lifecycle_client_role_base + 1U ||
                 role == ipc_lifecycle_client_role_base + 2U ||
                 role == ipc_lifecycle_client_role_base + 3U ||
                 role == ipc_lifecycle_client_role_base + 4U ||
                 role == ipc_lifecycle_client_role_base + 5U ||
                 role == ipc_lifecycle_client_role_base + 6U ||
                 role == ipc_lifecycle_client_role_base + 7U)
            name = "bin/pager-client";
        else if (role >= memory_client_image_role_base && role < memory_client_image_role_base + 3U)
            name = "bin/memory-client";
#endif
        else if (role >= control_plane_image_role_base &&
                 role < control_plane_image_role_base + control_plane_image_role_count)
            name = role == domain_manager_image_role   ? "bin/domain-manager"
                  : role == console_server_image_role ? "bin/console-server"
                                                        : "bin/control-plane";

        const auto* earlyfs_begin = reinterpret_cast<const u8*>(sys_arm64_earlyfs_image_start);
        const auto earlyfs_size = static_cast<usize_t>(sys_arm64_earlyfs_image_end -
                                                        sys_arm64_earlyfs_image_start);
        const auto found = platform::v1::earlyfs::find(earlyfs_begin, earlyfs_size, name);
        if (!found.valid())
            return {sys_arm64_earlyfs_image_start, sys_arm64_earlyfs_image_start};
        auto* start = const_cast<char*>(reinterpret_cast<const char*>(found.data));
        return {start, start + found.size};
    }
#else
    [[nodiscard]] inline image_view image_for_role(word_t) noexcept {
        return {sys_arm64_user_image_start, sys_arm64_user_image_end};
    }
#endif

    /*
     * Self-test entry point for the linked-in earlyfs image, called from
     * portable kernel test code (tests/include/sys/kernel/tests/earlyfs/
     * directory.hh) -- kept arch-specific because the underlying symbols
     * only exist on this arch/boot-profile combination; amd64 provides a
     * trivial matching stub so shared test code stays portable.
     */
#if CONFIG_ROOT_ONLY_BOOT
    [[nodiscard]] inline bool validate_earlyfs_image() noexcept {
        const auto* begin = reinterpret_cast<const u8*>(sys_arm64_earlyfs_image_start);
        const auto size =
            static_cast<usize_t>(sys_arm64_earlyfs_image_end - sys_arm64_earlyfs_image_start);
        if (!platform::v1::earlyfs::find(begin, size, "bin/init").valid())
            return false;
        if (!platform::v1::earlyfs::find(begin, size, "bin/memory-server").valid())
            return false;
        if (!platform::v1::earlyfs::find(begin, size, "bin/control-plane").valid())
            return false;
        if (!platform::v1::earlyfs::find(begin, size, "bin/domain-manager").valid())
            return false;
        if (platform::v1::earlyfs::find(begin, size, "no/such/entry").valid())
            return false;
        return true;
    }
#else
    [[nodiscard]] inline bool validate_earlyfs_image() noexcept {
        return true;
    }
#endif

    [[nodiscard]] inline constexpr vaddr_t user_image_end() noexcept {
        return user_code + elf64::bootstrap_size;
    }

    inline void synchronize_instruction_cache(void* start, usize_t size) noexcept {
        if (size == 0U)
            return;

        u64 ctr = 0U;
        __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
        const usize_t data_line = static_cast<usize_t>(4U)
                                  << static_cast<usize_t>((ctr >> 16U) & 0xfU);
        const usize_t instruction_line = static_cast<usize_t>(4U)
                                         << static_cast<usize_t>(ctr & 0xfU);
        const vaddr_t begin = reinterpret_cast<vaddr_t>(start);
        const vaddr_t end = begin + size;

        for (vaddr_t address = begin & ~(data_line - 1U); address < end; address += data_line) {
            __asm__ volatile("dc cvau, %0" : : "r"(address) : "memory");
        }
        __asm__ volatile("dsb ish" : : : "memory");
        /*
         * The address-space slot may previously have executed a different ELF
         * at the same virtual addresses on another CPU.  IC IVAU only
         * invalidates the local PE, so it is insufficient for slot reuse in
         * the SMP certification path.  Invalidate all instruction caches in
         * the inner-shareable domain after cleaning the replacement image to
         * the point of unification.
         */
        (void)instruction_line;
        __asm__ volatile("ic ialluis" : : : "memory");
        __asm__ volatile("dsb ish\n\tisb" : : : "memory");
    }

    // Re-exported so generic kernel code (sys::kernel::space::address_space)
    // can name these without knowing they're arm64's elf64 loader's types --
    // amd64 provides the same two names directly under sys::arch::space.
    using page_allocate_fn = elf64::page_allocate_fn;
    using page_release_fn = elf64::page_release_fn;

    struct address_space {
        memory::table_t l0{};
        memory::table_t l1{};
        memory::table_t l2{};
        memory::table_t l3{};
        paddr_t image_backing[elf64::bootstrap_pages]{};
        elf64::page_permissions image_permissions[elf64::bootstrap_pages]{};
        /*
         * Block 0's L3 is the embedded `l3` above. Blocks 1.. are allocated
         * from the physical page supply the first time something maps into
         * them, so a process that never grows a heap pays nothing for the
         * larger window -- which is the whole reason these are pointers and
         * not embedded tables. Sixteen embedded L3s would be 60 KiB per
         * address space, and there is one address space per user thread
         * slot, so the static cost would land whether or not anything used
         * it. l3_backing records what release() has to hand back.
         */
        memory::table_t* l3_tables[user_block_count]{};
        paddr_t l3_backing[user_block_count]{};
        /* The stack is frame-backed for the same reason: an embedded
         * 64 KiB array per slot would be paid by every server that only
         * ever needs one page of it. */
        paddr_t stack_backing[user_stack_pages]{};
        vaddr_t image_entry{user_code};
        error_t image_status{error_t::invalid_argument};
        u16 asid{};
        u32 asid_generation{};
        volatile u32 active_cpu_mask{};
    };

    /*
     * Releases any physical pages this slot's ELF image currently holds.
     * Bootstrap slots are reused across many process create/destroy cycles
     * (see the reuse note in activate() below), so this runs both when a
     * slot is torn down (release()) and defensively before a reused slot
     * loads a new image (initialize()) -- without it, dynamic frame-backed
     * loading would leak one physical page per slot per reuse.
     */
    inline void release_image_backing(address_space& value, elf64::page_release_fn release_page) noexcept {
        for (usize_t page = 0U; page < elf64::bootstrap_pages; ++page) {
            if (value.image_backing[page] != 0U) {
                (void)release_page(value.image_backing[page]);
                value.image_backing[page] = 0U;
            }
        }
    }

    /*
     * Returns every on-demand L3 table. Block 0 is the embedded `l3` and is
     * never released -- it has no backing page of its own. Runs on the same
     * two occasions as release_image_backing(): real teardown, and
     * defensively before a reused slot loads a new image, because a slot
     * that grew a heap once would otherwise leak its tables into the next
     * process to occupy it.
     */
    // Defined below, with the rest of the TLB maintenance helpers.
    inline void invalidate_asid(u16 asid) noexcept;

    inline void release_dynamic_tables(address_space& value,
                                       elf64::page_release_fn release_page) noexcept {
        bool detached = false;
        for (usize_t block = 1U; block < user_block_count; ++block) {
            if (value.l3_backing[block] == 0U) {
                value.l3_tables[block] = nullptr;
                continue;
            }
            /*
             * Clear the L2 entry BEFORE handing the page back. Freeing it
             * first leaves the table descriptor pointing at a page the
             * allocator is free to reissue, so a walk through this space --
             * speculative or real -- could translate through whatever the
             * next owner writes there. The page is only safe to release
             * once nothing can reach it.
             */
            const vaddr_t block_base = user_window_base + block * user_block_size;
            value.l2.entry[(block_base >> 21U) & 0x1ffU] = 0U;
            detached = true;
            value.l3_tables[block] = nullptr;
            (void)release_page(value.l3_backing[block]);
            value.l3_backing[block] = 0U;
        }
        if (detached)
            invalidate_asid(value.asid);
    }

    inline void release_stack_backing(address_space& value,
                                      elf64::page_release_fn release_page) noexcept {
        for (usize_t page = 0U; page < user_stack_pages; ++page) {
            if (value.stack_backing[page] != 0U) {
                (void)release_page(value.stack_backing[page]);
                value.stack_backing[page] = 0U;
            }
        }
    }

    [[nodiscard]] inline error_t initialize(address_space& value, word_t role,
                                            elf64::page_allocate_fn allocate_page,
                                            elf64::page_release_fn release_page) noexcept {
        release_image_backing(value, release_page);
        release_dynamic_tables(value, release_page);
        release_stack_backing(value, release_page);

        asid::handle identifier{};
        const error_t asid_result = asid::allocate(identifier);
        if (asid_result != error_t::success)
            return asid_result;
        value.asid = identifier.value;
        value.asid_generation = identifier.generation;
        value.active_cpu_mask = 0U;
        memory::build_kernel_table(value.l0, value.l1, value.l2);
        memory::clear(value.l3);
        const usize_t code_l2 = static_cast<usize_t>((user_code >> 21U) & 0x1ffU);
        value.l2.entry[code_l2] = memory::table_descriptor(value.l3);

        const image_view image = image_for_role(role);
        const usize_t image_size = static_cast<usize_t>(image.end - image.start);
        const elf64::result loaded = elf64::load_dynamic(
            reinterpret_cast<const u8*>(image.start), image_size, user_code, value.image_backing,
            value.image_permissions, allocate_page, release_page);
        value.image_status = loaded.status;
        value.image_entry = loaded.entry;
        if (loaded.status == error_t::success) {
            const usize_t first_index = static_cast<usize_t>((user_code >> 12U) & 0x1ffU);
            for (usize_t page = 0U; page < elf64::bootstrap_pages; ++page) {
                const auto permission = value.image_permissions[page];
                if (!permission.present)
                    continue;
                auto* page_address = reinterpret_cast<void*>(
                    static_cast<uintptr_t>(value.image_backing[page]));
                synchronize_instruction_cache(page_address, memory::page_size);
                u64 descriptor = (reinterpret_cast<u64>(page_address) & ~0xfffULL) |
                                 memory::descriptor_page | memory::access_flag |
                                 memory::inner_shareable | memory::attr_normal;
                descriptor |= permission.writable ? memory::ap_el0_rw : memory::ap_el0_ro;
                if (!permission.executable)
                    descriptor |= memory::pxn | memory::uxn;
                value.l3.entry[first_index + page] = descriptor;
            }
        }

        /*
         * The stack is now several frame-backed pages rather than one
         * embedded array. Allocation can fail, and a half-mapped stack is
         * worse than none: the thread would run with a plausible SP and
         * fault partway down. Report no_memory instead and let the caller
         * release the slot, which returns whatever was allocated here.
         */
        const usize_t stack_index = static_cast<usize_t>((user_stack_base >> 12U) & 0x1ffU);
        for (usize_t page = 0U; page < user_stack_pages; ++page) {
            paddr_t physical = 0U;
            if (allocate_page(physical) != error_t::success || physical == 0U) {
                value.image_status = error_t::no_memory;
                return error_t::no_memory;
            }
            value.stack_backing[page] = physical;
            value.l3.entry[stack_index + page] =
                (physical & ~0xfffULL) | memory::descriptor_page | memory::access_flag |
                memory::inner_shareable | memory::attr_normal | memory::ap_el0_rw | memory::pxn |
                memory::uxn;
        }
        return value.image_status;
    }

    inline void activate(address_space& value) noexcept {
        asid::handle identifier{value.asid, value.asid_generation};
        if (asid::refresh(identifier) != error_t::success)
            return;
        value.asid = identifier.value;
        value.asid_generation = identifier.generation;
        const u64 root = reinterpret_cast<u64>(&value.l0) & 0x0000ffffffffffffULL;
        const u64 ttbr = root | (static_cast<u64>(value.asid) << 48U);
        __atomic_fetch_or(&value.active_cpu_mask, 1U << arch::cpu::current_id(), __ATOMIC_RELEASE);
        /*
         * Address-space slots and ASIDs are reused by the bounded bootstrap
         * process pool.  Invalidate every cached stage-1 translation for the
         * ASID before installing the replacement root; otherwise a secondary
         * CPU may execute through a stale translation even though the new
         * page tables and image bytes are globally visible.
         */
        const u64 asid_operand = static_cast<u64>(value.asid) << 48U;
        __asm__ volatile("dsb ishst\n\t"
                         "tlbi aside1is, %1\n\t"
                         "dsb ish\n\t"
                         "msr ttbr0_el1, %0\n\t"
                         "isb"
                         :
                         : "r"(ttbr), "r"(asid_operand)
                         : "memory");
        /*
         * A bootstrap address-space slot can be destroyed, reloaded with a
         * different ELF, and then scheduled on a CPU that previously executed
         * the old image at the same VA/ASID.  The loader CPU cleans the new
         * bytes to PoU and broadcasts IC IALLUIS, but architectural completion
         * of that broadcast does not provide a convenient per-PE reuse
         * handshake for this bounded bootstrap scheduler.  Invalidate the
         * local instruction cache after installing TTBR0 and before returning
         * to PL3 so every target CPU observes the replacement image.
         *
         * This conservative whole-cache operation belongs only to the current
         * bounded bootstrap loader.  The production process/address-space
         * manager must replace it with generation-tracked residency and
         * targeted cross-CPU synchronization.
         */
        __asm__ volatile("ic iallu\n\tdsb nsh\n\tisb" : : : "memory");
    }

    inline void activate_kernel() noexcept {
        /*
         * Kernel execution currently shares TTBR0 with each bounded user
         * address space.  Before an address-space table may be reclaimed, a
         * CPU entering the EL1 idle context must switch back to the permanent
         * kernel root so teardown cannot remove the translation backing the
         * kernel instruction stream itself.
         */
        memory::activate(reinterpret_cast<paddr_t>(&memory::kernel_l0));
    }

    inline void release(address_space& value, elf64::page_release_fn release_page) noexcept {
        release_image_backing(value, release_page);
        release_dynamic_tables(value, release_page);
        release_stack_backing(value, release_page);
        asid::handle identifier{value.asid, value.asid_generation};
        asid::release(identifier);
        value.asid = 0U;
        value.asid_generation = 0U;
    }

    inline void invalidate_asid(u16 asid) noexcept {
        const u64 operand = static_cast<u64>(asid) << 48U;
        __asm__ volatile("dsb ishst\n\ttlbi aside1is, %0\n\tdsb ish\n\tisb"
                         :
                         : "r"(operand)
                         : "memory");
    }

    /*
     * The one page that must never carry a mapping: the stack's guard.
     * Everything else in the window is either already mapped (and so
     * rejected as busy) or legitimately available, which is why this is the
     * only address map_page() refuses by position rather than by state.
     */
    [[nodiscard]] inline constexpr bool is_guard_page(vaddr_t address) noexcept {
        return address == user_stack_base - memory::page_size;
    }

    [[nodiscard]] inline constexpr bool in_user_window(vaddr_t address) noexcept {
        return address >= user_window_base && address < user_window_end;
    }

    /*
     * Resolves the L3 table covering `address`, allocating it if this is the
     * first mapping into that 2 MiB block. Returns nullptr only when a table
     * was needed and could not be allocated; block 0 always resolves,
     * because its table is embedded in the address space itself.
     */
    [[nodiscard]] inline memory::table_t* resolve_l3(address_space& value, vaddr_t address,
                                                     elf64::page_allocate_fn allocate_page) noexcept {
        const usize_t block = static_cast<usize_t>((address - user_window_base) / user_block_size);
        if (block >= user_block_count)
            return nullptr;
        /*
         * Block 0's table is the embedded `l3`, structurally, so resolve it
         * without consulting l3_tables[]. The bootstrap root space is built
         * by a path that never calls initialize(), so anything that depends
         * on initialize() having populated a pointer here is wrong for
         * exactly the one address space that exists before userspace does.
         */
        if (block == 0U)
            return &value.l3;
        if (value.l3_tables[block] != nullptr)
            return value.l3_tables[block];
        if (allocate_page == nullptr)
            return nullptr;
        paddr_t physical = 0U;
        if (allocate_page(physical) != error_t::success || physical == 0U)
            return nullptr;
        auto* table = reinterpret_cast<memory::table_t*>(static_cast<uintptr_t>(physical));
        memory::clear(*table);
        value.l3_backing[block] = physical;
        value.l3_tables[block] = table;
        /*
         * Publish the table into the L2 only after it is cleared, so no CPU
         * can walk into a block full of whatever the page previously held.
         */
        arch::cpu::store_barrier();
        const usize_t l2_index = static_cast<usize_t>((address >> 21U) & 0x1ffU);
        value.l2.entry[l2_index] = memory::table_descriptor(*table);
        invalidate_asid(value.asid);
        return table;
    }

    [[nodiscard]] inline error_t map_page(address_space& value, vaddr_t address, void* page,
                                          bool writable, bool executable, bool device,
                                          bool inner_shareable_mapping,
                                          elf64::page_allocate_fn allocate_page) noexcept {
        if ((address & (memory::page_size - 1U)) != 0U || !in_user_window(address) ||
            is_guard_page(address) || (writable && executable))
            return error_t::invalid_argument;
        memory::table_t* table = resolve_l3(value, address, allocate_page);
        if (table == nullptr)
            return error_t::no_memory;
        const usize_t l3_index = static_cast<usize_t>((address >> 12U) & 0x1ffU);
        if (table->entry[l3_index] != 0U)
            return error_t::busy;
        u64 descriptor = (reinterpret_cast<u64>(page) & ~0xfffULL) | memory::descriptor_page |
                         memory::access_flag |
                         (inner_shareable_mapping ? memory::inner_shareable : 0ULL) |
                         (device ? memory::attr_device : memory::attr_normal);
        descriptor |= writable ? memory::ap_el0_rw : memory::ap_el0_ro;
        if (!executable)
            descriptor |= memory::pxn | memory::uxn;
        table->entry[l3_index] = descriptor;
        invalidate_asid(value.asid);
        return error_t::success;
    }

    [[nodiscard]] inline error_t unmap_page(address_space& value, vaddr_t address) noexcept {
        if ((address & (memory::page_size - 1U)) != 0U || !in_user_window(address))
            return error_t::invalid_argument;
        /* Never allocates: unmapping an address in a block that was never
         * populated is not-found, not a reason to build a table. */
        memory::table_t* table = resolve_l3(value, address, nullptr);
        if (table == nullptr)
            return error_t::not_found;
        const usize_t l3_index = static_cast<usize_t>((address >> 12U) & 0x1ffU);
        if (table->entry[l3_index] == 0U)
            return error_t::not_found;
        table->entry[l3_index] = 0U;
        invalidate_asid(value.asid);
        return error_t::success;
    }

    /*
     * Read-only descriptor lookup for the same window map_page() serves.
     * user_access.hh validates every user copy through this rather than
     * indexing a single embedded table, which stopped being correct once a
     * process could hold mappings outside block 0.
     */
    [[nodiscard]] inline u64 page_descriptor(const address_space& value,
                                             vaddr_t address) noexcept {
        if (!in_user_window(address))
            return 0U;
        const usize_t block = static_cast<usize_t>((address - user_window_base) / user_block_size);
        if (block >= user_block_count)
            return 0U;
        const memory::table_t* table =
            block == 0U ? &value.l3 : value.l3_tables[block];
        if (table == nullptr)
            return 0U;
        return table->entry[(address >> 12U) & 0x1ffU];
    }

    [[nodiscard]] inline vaddr_t entry(const address_space& value) noexcept {
        return value.image_entry;
    }
    [[nodiscard]] inline constexpr vaddr_t stack_top() noexcept {
        return user_stack_base + user_stack_size;
    }
} // namespace sys::arch::space
