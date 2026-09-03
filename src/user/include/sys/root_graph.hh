#pragma once

#include <sys/console_client.hh>
#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/guest_manifest.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/platform/v1/earlyfs.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/fault.hh>
#include <abi/sys/v1/memory.hh>
#include <abi/sys/v1/process.hh>
#include <abi/sys/v1/virtio.hh>

namespace sys::root_graph
{
    inline constexpr word_t selector_base = 33U;
    inline constexpr word_t selector_stride = 3U;
    inline constexpr capability_id_t root_notification = 14U;
    /*
     * Every launched child's fault-endpoint capability (its own slot 10) is
     * minted from root's own slot 10 (create_user_bundle(),
     * scheduler.hh:518), so root receiving on its own slot 10 gets every
     * child's fault delivery, badged per-child via endpoint_badge(). Root
     * itself was also given this same slot 10 as its own fault endpoint at
     * boot (scheduler.hh's root bootstrap).
     */
    inline constexpr capability_id_t root_fault_endpoint = 10U;
    inline constexpr capability_id_t self_space_selector = 3U;
    inline constexpr capability_id_t supervisor_space_selector = 151U;
    inline constexpr word_t memory_role = 0x100U;
    inline constexpr word_t memory_selector =
        selector_base + abi::v1::control_plane_role_count * selector_stride;
    inline constexpr capability_id_t endpoint_base = memory_selector + 3U;
    inline constexpr capability_id_t service_endpoint = 11U;
    /*
     * memory-server's own service endpoint, root's copy. Must be a
     * DIFFERENT object from root_fault_endpoint (10U): memory-server's
     * production request loop (memory_server_operation -- grant_frame/
     * query/shutdown) used to receive on slot 10 too, since that's the
     * fault-endpoint capability create_user_bundle() mints into every
     * role's slot 10 regardless -- memory_server_role additionally gets
     * read rights there so it can act as a pager. With nothing yet calling
     * its production API, that loop sat permanently blocked_receive on
     * slot 10, meaning it won essentially every fault-delivery race against
     * root/the supervision thread for ANY role's crash, not just genuine
     * page faults -- confirmed happening in practice while verifying
     * restart-on-fault (USR-034). Minted into memory-server's own slot 11
     * (service_endpoint) below, same convention every other role's own
     * service endpoint already uses.
     */
    inline constexpr capability_id_t memory_service_endpoint =
        endpoint_base + abi::v1::control_plane_role_count;

    /*
     * serial-driver, same non-control-plane-role wiring as memory-server
     * above: it doesn't fit the fixed 5-slot control_plane_role enum/loop,
     * so it gets a direct process_create in supervise() plus its own
     * selector range and endpoint, past memory's.
     */
    inline constexpr word_t serial_role = 0x107U;
    inline constexpr word_t serial_selector = memory_service_endpoint + 1U;
    inline constexpr capability_id_t serial_service_endpoint = serial_selector + 3U;

    static_assert(serial_service_endpoint < 64U);

    /*
     * virtio block driver, wired exactly like serial-driver above: not a
     * control_plane_role, so a direct process_create in supervise() plus its
     * own selectors and service endpoint. Placed past the leaf-0 band the
     * asserts above guard (cspace_leaf_slot_count is 64, and root's cspace
     * has 4 leaves -- supervisor_space_selector=151 already lives outside
     * leaf 0), because leaf 0 is nearly full and crowding it buys nothing.
     */
    inline constexpr word_t block_role = 0x109U;
    inline constexpr word_t block_selector = 100U;
    inline constexpr capability_id_t block_service_endpoint = 103U;
    inline constexpr capability_id_t block_mmio_root_frame_selector = 93U;
    inline constexpr capability_id_t block_mmio_child_frame_selector = 20U;
    inline constexpr capability_id_t block_irq_root_selector = 92U;
    inline constexpr capability_id_t block_irq_child_selector = 21U;
    inline constexpr capability_id_t block_serial_endpoint_selector = 23U;
    /*
     * Shared data buffer for block transfers. Deliberately a DIFFERENT frame
     * from the driver's own DMA page: that page holds the virtqueue
     * descriptor table and the available/used rings, so handing it to a
     * client would let the client corrupt the queue the driver is running.
     * This frame carries only payload bytes.
     *
     * Root creates it and mints it to the driver with control as well as
     * read/write, because the driver must call frame_physical_address on it
     * to point a descriptor at it, and that operation requires control. A
     * client gets read/write only -- enough to fill or drain a sector,
     * not enough to ask where it physically lives.
     */
    inline constexpr capability_id_t block_shared_root_frame_selector = 91U;
    inline constexpr capability_id_t block_shared_child_frame_selector = 25U;
    /*
     * INTID 79 = GIC SPI 47 = virtio-mmio transport 31, the transport QEMU
     * plugs the first attached device into (it allocates downward from the
     * top). Edge-rising, per the device tree's <0 47 1> -- unlike pl011's
     * level-high, so the driver must re-check the device's InterruptStatus
     * after acknowledging rather than expect another edge for work already
     * queued.
     *
     * Only this one IRQ is created, not one per transport in the granted
     * page: dynamic_interrupt_count is 16, so covering all 8 would spend
     * half the pool to serve a configuration nothing boots. The driver
     * verifies the transport it discovered matches this IRQ and falls back
     * to polling (reporting that it did) if a differently-populated machine
     * ever puts the device elsewhere.
     */
    inline constexpr irq_id_t block_irq = 79U;
    /*
     * The LAST page of the virtio-mmio window (transports 24..31), not the
     * first: QEMU populates these transports downward from the top, so a
     * single attached device lands on transport 31 at 0x0a003e00. See
     * platform/qemu_arm64_virt/memory.hh's virtio_mmio_grant_base.
     */
    inline constexpr word_t block_mmio_physical_address = 0x0a003000U;

    /*
     * Per-role restart bookkeeping (USR-034): thread_id lets drain_fault_
     * reports() correlate a fault's sender badge back to "which role
     * crashed"; restart_count feeds control_plane::may_restart()'s bound.
     * This has to live in genuine shared memory, not an ordinary global:
     * root's two threads (the original one and the supervision thread from
     * thread_create) share one cspace but NOT memory -- the supervision
     * thread has its own independent address space, loaded fresh from
     * bin/init's ELF image, so it has its own separate copy of every plain
     * global. thread_id/restart_count are declared volatile because they're
     * polled cross-thread with no lock: the only concurrent-write window is
     * during a restart, and every writer/reader pair below reasons
     * explicitly about ordering (see create_supervisor_state(),
     * map_supervisor_state_self(), restart_role(), run_embedded_guest_loop()).
     */
    struct role_entry final {
        volatile u32 thread_id{};
        volatile u32 restart_count{};
    };

    struct supervisor_state final {
        role_entry roles[abi::v1::control_plane_role_count];
    };

    inline constexpr capability_id_t supervisor_state_frame_selector = 96U;
    inline constexpr word_t supervisor_state_address = 0x20052000U;

    [[nodiscard]] inline supervisor_state* supervisor_state_ptr() noexcept {
        return reinterpret_cast<supervisor_state*>(
            static_cast<uintptr_t>(supervisor_state_address));
    }

    /*
     * Creates the shared page and maps it into root's OWN address space
     * only. The supervision thread maps the same frame into its own space
     * itself, as the very first thing it does (map_supervisor_state_self(),
     * called from supervision_thread_entry() before its drain loop) --
     * doing it that way instead of root mapping it into both spaces avoids
     * a race: the supervision thread could start running (and try to read
     * this page) the instant thread_create's IPI reschedules it, which
     * could be before root's own follow-up map_frame call for the second
     * space ever executes. Since both threads share one cspace, the
     * supervision thread already has a valid capability to this exact frame
     * object the moment it starts running -- it just maps it itself.
     */
    [[nodiscard]] inline bool create_supervisor_state() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const word_t attrs = abi::v1::encode_mapping_attributes(
            abi::v1::memory_type::normal, abi::v1::memory_shareability::inner_shareable);
        if (control(abi::v1::control_operation::frame_create, 0U,
                    supervisor_state_frame_selector) != success)
            return false;
        if (control(abi::v1::control_operation::map_frame, self_space_selector,
                    supervisor_state_frame_selector, supervisor_state_address, read_write,
                    attrs) != success)
            return false;
        for (auto& entry : supervisor_state_ptr()->roles) {
            entry.thread_id = 0U;
            entry.restart_count = 0U;
        }
        return true;
    }

    [[nodiscard]] inline bool map_supervisor_state_self() noexcept {
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const word_t attrs = abi::v1::encode_mapping_attributes(
            abi::v1::memory_type::normal, abi::v1::memory_shareability::inner_shareable);
        return control(abi::v1::control_operation::map_frame, supervisor_space_selector,
                       supervisor_state_frame_selector, supervisor_state_address, read_write,
                       attrs) == static_cast<word_t>(error_t::success);
    }

    [[nodiscard]] inline bool launch(word_t index, supervisor_state& state) noexcept {
        const word_t selector = selector_base + index * selector_stride;
        const word_t role = static_cast<word_t>(abi::v1::control_plane_role::process) + index;
        const capability_id_t endpoint = endpoint_base + index;
        constexpr word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
        if (control(abi::v1::control_operation::endpoint_create, endpoint) !=
            static_cast<word_t>(error_t::success))
            return false;
        word_t allocated_id = 0U;
        if (control_result1(allocated_id, abi::v1::control_operation::process_create, index % 4U,
                            role, selector, selector + 1U,
                            selector + 2U) != static_cast<word_t>(error_t::success))
            return false;
        if (control(abi::v1::control_operation::capability_mint, selector + 1U, service_endpoint,
                    endpoint, read_write, index + 1U) != static_cast<word_t>(error_t::success))
            return false;
        state.roles[index].thread_id = static_cast<u32>(allocated_id);
        state.roles[index].restart_count = 0U;
        return true;
    }

    inline constexpr word_t console_index =
        static_cast<word_t>(abi::v1::control_plane_role::console) -
        static_cast<word_t>(abi::v1::control_plane_role::process);

    /*
     * Bring-up progress reporting through console-server. supervise() had
     * no diagnostics past the console check, so any later failure was a
     * silent exit with a apparently-healthy boot log -- which is exactly how
     * the supervision-thread spawn failure below went unnoticed.
     */
    [[nodiscard]] inline bool healthy(word_t index) noexcept;

    inline void report(const char* text) noexcept {
        (void)console::write(endpoint_base + console_index, text);
    }

    /*
     * The UART itself now belongs entirely to serial-driver (src/user/
     * drivers/serial), not console-server -- device_frame_create() and
     * interrupt_create() are both root-gated, so root creates the one live
     * device frame and the one live IRQ capability for it (exclusivity-
     * checked the same way create_device_frame() already was) and mints
     * both into serial-driver's cspace at the fixed slots its own code
     * expects (uart_frame_selector=20, irq_selector=21). console_irq=33 is
     * the real PL011's GIC SPI 1, confirmed against this platform's actual
     * device tree (`pl011@9000000`'s `interrupts = <0 1 4>` -- SPI 1 =
     * INTID 33, flags 4 = level-high), not assumed.
     */
    inline constexpr capability_id_t serial_uart_root_frame_selector = 95U;
    inline constexpr capability_id_t serial_uart_child_frame_selector = 20U;
    inline constexpr word_t console_uart_physical_address = 0x09000000U;
    inline constexpr capability_id_t serial_irq_root_selector = 94U;
    inline constexpr capability_id_t serial_irq_child_selector = 21U;
    inline constexpr irq_id_t console_irq = 33U;

    // Both must run exactly once: device_frame_create()'s and
    // interrupt_create()'s exclusivity checks reject a second live
    // frame/IRQ capability for the same physical address/IRQ number.
    // serial-driver is not restart-covered (see restart_role()'s comment),
    // so there is no repeatable re-mint counterpart to design for here.
    [[nodiscard]] inline bool create_serial_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        if (control(abi::v1::control_operation::device_frame_create,
                    serial_uart_root_frame_selector, console_uart_physical_address) != success)
            return false;
        return control(abi::v1::control_operation::interrupt_create, serial_irq_root_selector,
                       console_irq, 0U) == success;
    }

    [[nodiscard]] inline bool mint_serial_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const word_t write_control = static_cast<word_t>(abi::v1::CapabilityRight::write) |
                                     static_cast<word_t>(abi::v1::CapabilityRight::control);
        const capability_id_t serial_task = serial_selector + 1U;
        if (control(abi::v1::control_operation::capability_mint, serial_task,
                    serial_uart_child_frame_selector, serial_uart_root_frame_selector,
                    read_write) != success)
            return false;
        return control(abi::v1::control_operation::capability_mint, serial_task,
                       serial_irq_child_selector, serial_irq_root_selector,
                       write_control) == success;
    }

    /*
     * console-server no longer touches hardware at all -- it forwards
     * TX/RX to serial-driver over IPC. It holds two capabilities beyond
     * its own service_endpoint=11 (the existing write/health/describe/stop
     * endpoint, unchanged): a capability to serial-driver's own service
     * endpoint (console_serial_endpoint_selector, so it can forward), and
     * a second, DEDICATED endpoint of its own for read_byte
     * (console_stdin_endpoint_selector) -- served by a second thread (see
     * console_stdin_role below), independent from the write-serving main
     * thread. Both are re-mintable, called at launch and replayed by
     * restart_role() on a console-server restart, same as every other
     * role-specific extra mint.
     */
    inline constexpr capability_id_t console_serial_endpoint_selector = 20U;
    inline constexpr capability_id_t console_stdin_endpoint_selector = 12U;
    inline constexpr capability_id_t console_stdin_endpoint = serial_service_endpoint + 1U;

    static_assert(console_stdin_endpoint < 64U);

    /*
     * console-server's second thread: bound to its OWN already-built binary
     * under a reserved role, exactly like root_supervisor_role does for
     * root's own second thread (see that constant's comment below).
     * console-server spawns it itself via thread_create once root has
     * bound this role's image -- thread_create is not root-gated, only
     * role_image_bind is, so console-server can self-service the create
     * call. thread_selector/space_selector are console-server's own
     * cspace slots (shared with its main thread, since thread_create
     * attaches to the caller's existing task).
     */
    inline constexpr word_t console_stdin_role = 0x108U;
    inline constexpr capability_id_t console_stdin_thread_selector = 30U;
    inline constexpr capability_id_t console_stdin_space_selector = 31U;

    /*
     * Block driver resources. Only the MMIO device frame is created here --
     * deliberately no interrupt_create yet: which virtio-mmio transport slot
     * QEMU plugs a device into is not answerable from the device tree (every
     * slot is present and reads valid magic regardless), so the IRQ to
     * request (INTID 48 + slot) isn't known until the driver has probed.
     * dynamic_interrupt_count is 16, so speculatively burning 8 of them on a
     * guess would cost half the pool. The driver reports the slot it found;
     * the IRQ capability follows once that answer is in.
     *
     * One-shot like create_serial_resources(), for the same reason:
     * device_frame_create()'s exclusivity check rejects a second live frame
     * for the same physical page.
     */
    [[nodiscard]] inline bool create_block_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        if (control(abi::v1::control_operation::device_frame_create, block_mmio_root_frame_selector,
                    block_mmio_physical_address) != success)
            return false;
        // Third argument non-zero selects edge triggering, matching the
        // device tree's <0 47 1>.
        if (control(abi::v1::control_operation::interrupt_create, block_irq_root_selector,
                    block_irq, 1U) != success)
            return false;
        // create_frame already assigns a physical page (it calls
        // assign_frame internally), so there is no separate allocate step --
        // calling frame_allocate on top of this would fail with busy.
        return control(abi::v1::control_operation::frame_create, 0U,
                       block_shared_root_frame_selector) == success;
    }

    [[nodiscard]] inline bool mint_block_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const capability_id_t block_task = block_selector + 1U;
        if (control(abi::v1::control_operation::capability_mint, block_task,
                    block_mmio_child_frame_selector, block_mmio_root_frame_selector,
                    read_write) != success)
            return false;
        const word_t write_control = static_cast<word_t>(abi::v1::CapabilityRight::write) |
                                     static_cast<word_t>(abi::v1::CapabilityRight::control);
        if (control(abi::v1::control_operation::capability_mint, block_task,
                    block_irq_child_selector, block_irq_root_selector, write_control) != success)
            return false;
        // Control as well as read/write: the driver must call
        // frame_physical_address on this to aim a virtqueue descriptor at
        // it, and that operation is gated on the control right.
        const word_t read_write_control =
            read_write | static_cast<word_t>(abi::v1::CapabilityRight::control);
        if (control(abi::v1::control_operation::capability_mint, block_task,
                    block_shared_child_frame_selector, block_shared_root_frame_selector,
                    read_write_control) != success)
            return false;
        // Diagnostics path: the driver reports its probe results as text
        // through serial-driver, the same endpoint console-server forwards
        // to, so a bring-up scan is actually observable on the console.
        return control(abi::v1::control_operation::capability_mint, block_task,
                       block_serial_endpoint_selector, serial_service_endpoint,
                       read_write) == success;
    }

    /*
     * Proves the block service works from a DIFFERENT task than the driver.
     * The driver's own boot self-check exercises its virtqueue, but entirely
     * inside the driver -- it says nothing about whether the IPC service or
     * the shared-frame capability wiring actually work for a client.
     *
     * Root is a genuine client here, not a synthetic one: it created the
     * payload frame, so it already holds it (no new capability is minted for
     * this), and it holds the driver's service endpoint because it created
     * that too. Same shape as the console-server check below, which likewise
     * proves a service end to end from root rather than trusting a badge.
     *
     * Sector 0 deliberately, not the last sector: the driver's self-check
     * uses the last one, so a distinct sector proves this transfer rather
     * than re-reading bytes the driver already left in the buffer.
     */
    inline constexpr word_t block_scratch_address = 0x20051000U;
    inline constexpr word_t block_probe_sector = 0U;
    inline constexpr u64 block_probe_seed = 0x726f6f74626c6b00ULL; // "rootblk\0"

    [[nodiscard]] inline bool map_block_buffer() noexcept {
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        // A normal RAM frame must be normal + inner-shareable
        // (memory::valid_attributes()), unlike the device frames above.
        const word_t attrs = abi::v1::encode_mapping_attributes(
            abi::v1::memory_type::normal, abi::v1::memory_shareability::inner_shareable);
        return control(abi::v1::control_operation::map_frame, self_space_selector,
                       block_shared_root_frame_selector, block_scratch_address, read_write,
                       attrs) == static_cast<word_t>(error_t::success);
    }

    [[nodiscard]] inline volatile u64* block_buffer(word_t offset) noexcept {
        return reinterpret_cast<volatile u64*>(static_cast<uintptr_t>(block_scratch_address) +
                                               offset);
    }

    /*
     * Absent is NOT a failure. A machine booted with no disk attached
     * (tools/run/run.sh's BLOCK_IMAGE=-) has a driver that came up fine and
     * correctly found nothing, and refusing to boot over that would make an
     * optional device mandatory. Only a device that is present but does not
     * work is a failure.
     */
    enum class block_check { absent, verified, failed };

    [[nodiscard]] inline block_check verify_block_service() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto capacity = ipc_call(block_service_endpoint,
                                       static_cast<word_t>(abi::v1::block_operation::info), 0U);
        if (capacity.status != success)
            return block_check::failed;
        // The driver reports not_found when it discovered no block device.
        if (capacity.message0 == static_cast<word_t>(error_t::not_found))
            return block_check::absent;
        if (capacity.message0 != success || capacity.message1 == 0U ||
            capacity.message2 != abi::v1::block_sector_size)
            return block_check::failed;

        // Fill, write, clear, read back. Clearing in between is what makes
        // this a real round trip rather than a check that the buffer still
        // holds what was just put there.
        for (word_t offset = 0U; offset < abi::v1::block_sector_size; offset += 8U)
            *block_buffer(offset) = block_probe_seed ^ offset;
        const auto wrote =
            ipc_call(block_service_endpoint, static_cast<word_t>(abi::v1::block_operation::write),
                     block_probe_sector);
        if (wrote.status != success || wrote.message0 != success)
            return block_check::failed;

        for (word_t offset = 0U; offset < abi::v1::block_sector_size; offset += 8U)
            *block_buffer(offset) = 0U;
        const auto read =
            ipc_call(block_service_endpoint, static_cast<word_t>(abi::v1::block_operation::read),
                     block_probe_sector);
        if (read.status != success || read.message0 != success)
            return block_check::failed;

        for (word_t offset = 0U; offset < abi::v1::block_sector_size; offset += 8U)
            if (*block_buffer(offset) != (block_probe_seed ^ offset))
                return block_check::failed;
        return block_check::verified;
    }

    [[nodiscard]] inline bool mint_console_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const capability_id_t console_task = selector_base + console_index * selector_stride + 1U;
        if (control(abi::v1::control_operation::capability_mint, console_task,
                    console_serial_endpoint_selector, serial_service_endpoint,
                    read_write) != success)
            return false;
        return control(abi::v1::control_operation::capability_mint, console_task,
                       console_stdin_endpoint_selector, console_stdin_endpoint,
                       read_write) == success;
    }

#if CONFIG_FAULT_INJECTION
    /*
     * Proves the supervision thread actually does its job, not merely that
     * it started. Spawning it and restarting a crashed role are different
     * properties, and only the first was ever observable -- the release
     * profile ran without a supervision thread at all for some time, and a
     * "graph ready" marker alone would not have caught a thread that starts
     * and then supervises nothing.
     *
     * Crashes the device role: it is restart-covered, and unlike console it
     * is not the path this reports through, nor is it the domain role
     * hosting a guest.
     */
    inline constexpr word_t device_index =
        static_cast<word_t>(abi::v1::control_plane_role::device) -
        static_cast<word_t>(abi::v1::control_plane_role::process);

    [[nodiscard]] inline bool verify_restart_on_fault(supervisor_state& state) noexcept {
        const u32 before = __atomic_load_n(&state.roles[device_index].thread_id, __ATOMIC_ACQUIRE);
        const u32 restarts_before =
            __atomic_load_n(&state.roles[device_index].restart_count, __ATOMIC_ACQUIRE);

        // The role dies instead of replying, so this must not block forever.
        const abi::v1::ipc_timeout timeout{.ticks = 16U, .enabled = true};
        (void)ipc_call(endpoint_base + device_index,
                       static_cast<word_t>(abi::v1::control_plane_operation::inject_fault), 0U, 0U,
                       0U, {}, timeout);

        /*
         * Wait for the supervision thread to observe the fault and restart
         * the role. Bounded: a hang here would otherwise look like a healthy
         * boot that simply never printed anything more.
         */
        constexpr u32 rounds = 200000000U;
        for (u32 round = 0U; round < rounds; ++round) {
            const u32 restarts =
                __atomic_load_n(&state.roles[device_index].restart_count, __ATOMIC_ACQUIRE);
            if (restarts != restarts_before) {
                // Restarted; confirm the replacement answers its service
                // endpoint, so this proves a working role rather than just
                // an incremented counter.
                const u32 after =
                    __atomic_load_n(&state.roles[device_index].thread_id, __ATOMIC_ACQUIRE);
                (void)before;
                (void)after;
                return healthy(device_index);
            }
        }
        return false;
    }
#endif

    [[nodiscard]] inline bool healthy(word_t index) noexcept {
        const auto reply =
            ipc_call(endpoint_base + index,
                     static_cast<word_t>(abi::v1::control_plane_operation::health), 0U);
        const word_t role = static_cast<word_t>(abi::v1::control_plane_role::process) + index;
        return reply.status == static_cast<word_t>(error_t::success) &&
               reply.message0 == abi::v1::control_plane_health_magic && reply.message1 == role;
    }

    inline constexpr word_t domain_index =
        static_cast<word_t>(abi::v1::control_plane_role::domain) -
        static_cast<word_t>(abi::v1::control_plane_role::process);

#if CONFIG_GUEST_EMBEDDED_IMAGE
    inline constexpr capability_id_t device_frame_base = 100U;
    inline constexpr capability_id_t device_irq_base = 116U;
    inline constexpr capability_id_t domain_console_endpoint_selector = 19U;
    inline constexpr capability_id_t domain_console_stdin_endpoint_selector = 52U;

    /*
     * Root owns physical devices/interrupts and creates capabilities for
     * them -- mirroring how it already creates the guest's stage-2 memory.
     * Which devices and IRQs a guest needs is read from its manifest, not
     * hardcoded here: root has no idea which guest it's booting. Must run
     * exactly once: device_frame_create()'s exclusivity check (like the
     * console UART's) rejects a second live frame at the same physical
     * address, and these objects live in root's own cspace, surviving a
     * domain-manager restart untouched -- restarting only needs to re-mint
     * them (mint_embedded_guest_resources() below), never recreate them.
     */
    [[nodiscard]] inline bool create_embedded_guest_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto& manifest = ::sys_arm64_domain_guest_manifest;
        if (!guest_manifest::valid(manifest) ||
            manifest.device_count > guest_manifest::maximum_devices)
            return false;
        for (word_t index = 0U; index < manifest.device_count; ++index) {
            const auto& dev = manifest.devices[index];
            if (control(abi::v1::control_operation::device_frame_create, device_frame_base + index,
                        dev.ipa) != success)
                return false;
            if (dev.forward_irq == guest_manifest::no_irq)
                continue;
            if (control(abi::v1::control_operation::interrupt_create, device_irq_base + index,
                        dev.forward_irq, dev.forward_trigger) != success)
                return false;
        }
        return true;
    }

    /*
     * Mints the device/IRQ/console-endpoint capabilities into the CURRENT
     * domain-manager task. Callable repeatedly: once at boot, and once per
     * restart (a freshly recreated domain-manager task has an empty
     * cspace, same as any other freshly created role).
     *
     * The guest's UART is virtual (vPL011, see domain/main.cc) rather than
     * passed through -- serial-driver (see mint_serial_resources() above)
     * owns the real hardware exclusively. The domain-manager needs its own
     * capabilities to call console-server's endpoints directly (write to
     * forward TX, and now a SEPARATE endpoint to poll RX -- console-server
     * splits write and read across two threads/endpoints, see
     * mint_console_resources()) rather than going through root. These mint
     * second capabilities to the exact same endpoint objects root already
     * holds from console-server's own launch() -- no new endpoints are
     * created, matching how every other inter-service capability in this
     * codebase is derived from an already-created source.
     */
    [[nodiscard]] inline bool mint_embedded_guest_resources() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const word_t write_control = static_cast<word_t>(abi::v1::CapabilityRight::write) |
                                     static_cast<word_t>(abi::v1::CapabilityRight::control);
        const word_t write_only = static_cast<word_t>(abi::v1::CapabilityRight::write);
        const capability_id_t domain_task = selector_base + domain_index * selector_stride + 1U;
        const auto& manifest = ::sys_arm64_domain_guest_manifest;
        if (!guest_manifest::valid(manifest) ||
            manifest.device_count > guest_manifest::maximum_devices)
            return false;
        for (word_t index = 0U; index < manifest.device_count; ++index) {
            const auto& dev = manifest.devices[index];
            if (control(abi::v1::control_operation::capability_mint, domain_task,
                        device_frame_base + index, device_frame_base + index,
                        read_write) != success)
                return false;
            if (dev.forward_irq == guest_manifest::no_irq)
                continue;
            if (control(abi::v1::control_operation::capability_mint, domain_task,
                        device_irq_base + index, device_irq_base + index, write_control) != success)
                return false;
        }
        if (control(abi::v1::control_operation::capability_mint, domain_task,
                    domain_console_endpoint_selector, endpoint_base + console_index,
                    write_only) != success)
            return false;
        return control(abi::v1::control_operation::capability_mint, domain_task,
                       domain_console_stdin_endpoint_selector, console_stdin_endpoint,
                       write_only) == success;
    }
#endif

    /*
     * Root-driven path lookup, wired up: resolves each PL3 server role's
     * binary by name in the earlyfs image (the same one the kernel embeds
     * and image_for_role() falls back to) and registers the result via
     * role_image_bind(), so the kernel/arch layer no longer needs to know
     * "role 0x203 means bin/domain-manager" for these roles -- it only
     * knows there's a (bounds-checked) span bound to that role number. Runs
     * once, before any process_create call, so every role is already bound
     * by the time image_for_role() is consulted for it.
     */
    inline constexpr capability_id_t earlyfs_frame_selector = 90U;
    /*
     * Scratch mappings live above the stack, in the region that opened up
     * once an address space stopped being one 2 MiB L3 with only its first
     * 64 pages reachable.
     *
     * They used to sit in the top pages of the image window, and the
     * highest of them (0x2003f000) was exactly user_stack_base - one page:
     * the stack guard. elf64::stack_guard_pages reserves that page from the
     * image loader precisely so a stack overflow faults instead of quietly
     * eating the image's last page -- but every process that mapped a
     * scratch page there filled the guard in by hand, so the guarantee only
     * ever held for processes that did no scratch mapping at all. Nothing
     * caught it because the stack was a single page and never came close to
     * overflowing into it.
     *
     * Sitting inside the image window was also fragile in its own right:
     * these addresses were free only because every current binary is small
     * enough that its ELF segments stop short of them.
     */
    inline constexpr word_t earlyfs_scratch_address = 0x20053000U;

    struct role_image_entry final {
        word_t role;
        const char* name;
    };

    [[nodiscard]] inline bool bind_role_images() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        if (control(abi::v1::control_operation::earlyfs_frame_create, earlyfs_frame_selector, 0U) !=
            success)
            return false;
        const word_t read_only = static_cast<word_t>(abi::v1::CapabilityRight::read);
        // create_earlyfs_frame() marks the frame device=true (its physical
        // address is a fixed kernel-image address, not pool-allocator
        // backed -- same reason create_device_frame() sets it), and
        // memory::valid_attributes() requires memory_type::device with
        // outer/non-shareable for any frame with that flag, regardless of
        // what the underlying memory actually is.
        const word_t attrs = abi::v1::encode_mapping_attributes(
            abi::v1::memory_type::device, abi::v1::memory_shareability::non_shareable);
        if (control(abi::v1::control_operation::map_frame, self_space_selector,
                    earlyfs_frame_selector, earlyfs_scratch_address, read_only, attrs) != success)
            return false;

        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U; // one page: header + directory always fit here

        const role_image_entry bindings[] = {
            {memory_role, "bin/memory-server"},
            {serial_role, "bin/serial-driver"},
            {block_role, "bin/virtio-driver"},
            {static_cast<word_t>(abi::v1::control_plane_role::process), "bin/control-plane"},
            {static_cast<word_t>(abi::v1::control_plane_role::device), "bin/control-plane"},
            {static_cast<word_t>(abi::v1::control_plane_role::console), "bin/console-server"},
            {console_stdin_role, "bin/console-server"},
            {static_cast<word_t>(abi::v1::control_plane_role::domain), "bin/domain-manager"},
            {static_cast<word_t>(abi::v1::control_plane_role::supervisor), "bin/control-plane"},
        };
        for (const auto& entry : bindings) {
            // find_span() only reads the directory (mapped here, one page) and
            // returns the entry's raw offset/size without dereferencing its
            // data -- the real binary data lives well beyond this one mapped
            // page. role_image_bind() independently re-validates the span
            // against the kernel's own known full image size.
            const auto found = platform::v1::earlyfs::find_span(page, directory_bound, entry.name);
            if (!found.found)
                return false;
            if (control(abi::v1::control_operation::role_image_bind, entry.role,
                        static_cast<word_t>(found.offset),
                        static_cast<word_t>(found.size)) != success)
                return false;
        }
        return true;
    }

    /*
     * Path-based process launch: resolves an arbitrary earlyfs-resident
     * path at runtime (not just the six roles bind_role_images() resolves
     * once at boot) and launches it as a new process. Reuses the earlyfs
     * frame bind_role_images() already mapped at earlyfs_scratch_address --
     * that mapping is never torn down, so no new frame/map_frame calls are
     * needed here.
     *
     * dynamic_launch_role is a single reserved role slot (clear of every
     * role range used elsewhere: 0x100-0x117, 0x200-0x204). role_image_bind
     * upserts by matching role first, so calling this again with a
     * different path simply repoints this one role_bindings[] slot at the
     * new path -- this deliberately supports only one dynamically-launched
     * image resolvable at a time. Two different dynamically-launched
     * programs cannot both remain independently re-launchable
     * simultaneously without a second reserved role or an unbind
     * operation, neither of which exists today; that is a known scope
     * limit, not an oversight.
     */
    inline constexpr word_t dynamic_launch_role = 0x300U;

    /*
     * A pool of reserved roles for dynamically launched programs, one per
     * live child, clear of every statically assigned range (0x100-0x117,
     * 0x200-0x204, 0x300-0x301).
     *
     * dynamic_launch_role above is a single slot, and role_image_bind
     * upserts by role, so binding a second path through it silently
     * repointed the first -- two dynamically launched programs could not
     * both stay relaunchable. That was an accepted limit while nothing
     * launched more than one thing at a time. A shell running commands is
     * exactly the case it does not survive, so it draws from here instead.
     */
    inline constexpr word_t dynamic_role_base = 0x400U;
    /*
     * Bounded by root's cspace, not by roles. Each live child costs root
     * four capability slots (thread, task, space, argument frame), a cspace
     * holds 256 (capability/cspace.hh's 4x64 radix), and root's own graph
     * already reaches selector 151 -- so eight children is 32 slots from
     * 160 with room left over, while the role table could hold far more.
     */
    inline constexpr word_t dynamic_role_count = 8U;
    inline constexpr capability_id_t dynamic_selector_base = 160U;
    inline constexpr capability_id_t dynamic_selectors_per_child = 4U;
    static_assert(dynamic_role_count <= 64U - 9U,
                  "the pool plus the statically bound roles must fit arch role_bindings[]");
    static_assert(dynamic_selector_base + dynamic_role_count * dynamic_selectors_per_child <= 256U,
                  "child selectors must fit root's cspace");

    struct child_slot {
        word_t role;
        capability_id_t thread;
        capability_id_t task;
        capability_id_t space;
        capability_id_t args_frame;
    };

    [[nodiscard]] inline constexpr child_slot slot_for(word_t index) noexcept {
        const capability_id_t base =
            dynamic_selector_base + static_cast<capability_id_t>(index) * dynamic_selectors_per_child;
        return {dynamic_role_base + index, base, static_cast<capability_id_t>(base + 1U),
                static_cast<capability_id_t>(base + 2U), static_cast<capability_id_t>(base + 3U)};
    }

    /*
     * Root has exactly one thread at boot (kernel bootstrap gives it
     * initial_threads=1), and in the embedded-guest production
     * configuration, that one thread ends up permanently blocked inside
     * start_embedded_guest()'s call to the domain-manager's serve
     * operation -- which, per domain/main.cc's serve() loop, does not
     * return until the guest hits a genuinely terminal VM exit, i.e.
     * effectively never for a healthy guest. From that point on root was
     * not polling drain_fault_reports() at all: any fault in any other
     * service got zero application-level handling, falling back entirely
     * to the kernel's own 500-tick fault timeout.
     *
     * root_supervisor_role gives root a second thread (via thread_create,
     * which attaches a new thread to the CALLER's own existing task rather
     * than allocating a new one -- see create_user_thread() in
     * thread/scheduler.hh) that runs root's own binary (bin/init is
     * already earlyfs-addressable, see image.mk) under this reserved role
     * instead of root's normal bootstrap entry, and does nothing but drain
     * faults for the rest of the system's uptime. It shares root's cspace
     * (and thus its capabilities and root authority) automatically --
     * thread_create needs no minting step for that, unlike process_create.
     * USR-034 (restart-on-fault) builds on this: drain_fault_reports() below
     * now correlates a fault's sender badge back to a role via the shared
     * supervisor_state page and restarts it, bounded by
     * control_plane::may_restart().
     */
    inline constexpr word_t root_supervisor_role = 0x301U;
    inline constexpr capability_id_t supervisor_thread_selector = 150U;

    // Defined below, next to the fault-endpoint constants they depend on.
    inline void drain_fault_reports(supervisor_state* state) noexcept;
    [[nodiscard]] inline bool restart_role(supervisor_state& state, word_t index) noexcept;

    /*
     * map_supervisor_state_self() must run before this thread ever touches
     * shared state -- see create_supervisor_state()'s comment for why root
     * doesn't map the second half itself. A failure here is treated as
     * "no shared state available" rather than aborting: drain_fault_reports()
     * degrades to reply-only (today's pre-USR-034 behavior) when passed
     * nullptr, so a crashed mapping still leaves fault reaping intact, just
     * without restart.
     */
    [[noreturn]] inline void supervision_thread_entry() noexcept {
        supervisor_state* const state =
            map_supervisor_state_self() ? supervisor_state_ptr() : nullptr;
        for (;;)
            drain_fault_reports(state);
    }

    [[nodiscard]] inline bool spawn_supervision_thread() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U;
        const auto found = platform::v1::earlyfs::find_span(page, directory_bound, "bin/init");
        if (!found.found) {
            report("sup: no bin/init\n");
            return false;
        }
        if (control(abi::v1::control_operation::role_image_bind, root_supervisor_role,
                    static_cast<word_t>(found.offset),
                    static_cast<word_t>(found.size)) != success) {
            report("sup: bind failed\n");
            return false;
        }
        /*
         * Exhausting the thread pool here used to be silent: supervise()
         * returned 7 and init exited, leaving a boot that looked healthy
         * but had no supervision thread and therefore no restart-on-fault.
         * See user_thread_count in kernel/thread/scheduler.hh.
         */
        if (control(abi::v1::control_operation::thread_create, 0U, root_supervisor_role,
                    supervisor_thread_selector, supervisor_space_selector) != success) {
            report("sup: no thread\n");
            return false;
        }
        return true;
    }

    [[nodiscard]] inline bool launch_path(const char* name, word_t cpu,
                                          capability_id_t thread_selector,
                                          capability_id_t task_selector,
                                          capability_id_t space_selector) noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U;
        const auto found = platform::v1::earlyfs::find_span(page, directory_bound, name);
        if (!found.found)
            return false;
        if (control(abi::v1::control_operation::role_image_bind, dynamic_launch_role,
                    static_cast<word_t>(found.offset), static_cast<word_t>(found.size)) != success)
            return false;
        return control(abi::v1::control_operation::process_create, cpu, dynamic_launch_role,
                       thread_selector, task_selector, space_selector) == success;
    }

    /*
     * Scratch page where root assembles an argument block before handing
     * the frame to the child. Distinct from every other scratch address so
     * assembling one cannot disturb an in-flight earlyfs or block mapping.
     */
    inline constexpr word_t args_build_address = 0x20054000U;

    /*
     * Writes an argv/envp block into a frame root owns, then leaves the
     * frame ready to mint. The child maps it itself -- see
     * native::args_frame for why the capability travels rather than the
     * mapping.
     *
     * Returns false without leaving a partial block if anything does not
     * fit, so a caller can report "argument list too long" rather than
     * launching a program with a silently truncated argv.
     */
    [[nodiscard]] inline bool write_args_block(capability_id_t frame_selector, const char* const* argv,
                                               word_t argc, const char* const* envp,
                                               word_t envc) noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        const word_t attrs = abi::v1::encode_mapping_attributes(
            abi::v1::memory_type::normal, abi::v1::memory_shareability::inner_shareable);
        if (argc + envc > abi::v1::process_args_max_entries)
            return false;
        if (control(abi::v1::control_operation::map_frame, self_space_selector, frame_selector,
                    args_build_address, read_write, attrs) != success)
            return false;

        auto* const header =
            reinterpret_cast<abi::v1::process_args_header*>(static_cast<uintptr_t>(args_build_address));
        auto* const base = reinterpret_cast<char*>(static_cast<uintptr_t>(args_build_address));
        for (usize_t index = 0U; index < abi::v1::process_args_size; ++index)
            base[index] = '\0';

        header->magic = abi::v1::process_args_magic;
        header->version = abi::v1::process_args_version;
        header->argc = static_cast<u32>(argc);
        header->envc = static_cast<u32>(envc);
        header->bytes_offset = static_cast<u32>(sizeof(abi::v1::process_args_header));
        header->bytes_size = 0U;

        char* const bytes = base + header->bytes_offset;
        const u32 capacity = static_cast<u32>(abi::v1::process_args_size) - header->bytes_offset;
        u32 cursor = 0U;
        bool overflowed = false;
        const auto append = [&](const char* text, u32 slot) noexcept {
            header->entries[slot] = cursor;
            for (const char* character = text; *character != '\0'; ++character) {
                if (cursor >= capacity) {
                    overflowed = true;
                    return;
                }
                bytes[cursor++] = *character;
            }
            if (cursor >= capacity) {
                overflowed = true;
                return;
            }
            bytes[cursor++] = '\0';
        };
        for (word_t index = 0U; index < argc && !overflowed; ++index)
            append(argv[index], static_cast<u32>(index));
        for (word_t index = 0U; index < envc && !overflowed; ++index)
            append(envp[index], static_cast<u32>(argc + index));
        header->bytes_size = cursor;

        const bool unmapped = control(abi::v1::control_operation::unmap_frame, self_space_selector,
                                      frame_selector, args_build_address) == success;
        return !overflowed && unmapped;
    }

    /*
     * Launches an earlyfs-resident program with arguments, into one of the
     * bounded child slots.
     *
     * Order matters and is not interchangeable: the argument frame is
     * created and filled BEFORE process_create, because filling it needs a
     * mapping into root's own space and root must not still hold that
     * mapping when the child starts. The mint into the child necessarily
     * comes after, since the child's cspace does not exist until then --
     * which is exactly the race the child's own bounded retry absorbs (see
     * native::args_frame).
     */
    [[nodiscard]] inline bool spawn(word_t index, const char* path, const char* const* argv,
                                    word_t argc, const char* const* envp, word_t envc,
                                    word_t cpu) noexcept {
        if (index >= dynamic_role_count)
            return false;
        const word_t success = static_cast<word_t>(error_t::success);
        const child_slot slot = slot_for(index);
        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U;
        const auto found = platform::v1::earlyfs::find_span(page, directory_bound, path);
        if (!found.found)
            return false;
        if (control(abi::v1::control_operation::role_image_bind, slot.role,
                    static_cast<word_t>(found.offset), static_cast<word_t>(found.size)) != success)
            return false;

        (void)control(abi::v1::control_operation::frame_destroy, slot.args_frame);
        /* 0 selects the caller's own task, the same form create_supervisor_
         * state() uses for root's own frames. */
        if (control(abi::v1::control_operation::frame_create, 0U, slot.args_frame) != success)
            return false;
        if (!write_args_block(slot.args_frame, argv, argc, envp, envc)) {
            (void)control(abi::v1::control_operation::frame_destroy, slot.args_frame);
            return false;
        }

        if (control(abi::v1::control_operation::process_create, cpu, slot.role, slot.thread,
                    slot.task, slot.space) != success) {
            (void)control(abi::v1::control_operation::frame_destroy, slot.args_frame);
            return false;
        }
        /*
         * Minted read|write even though the child maps it read-only:
         * map_frame demands the write right on the frame capability itself
         * to authorise mapping at all, independently of the permissions
         * requested for the mapping (see control.hh's map_frame case). The
         * child's mapping is still read-only, so it cannot modify its own
         * arguments through the address it reads them at.
         */
        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        return control(abi::v1::control_operation::capability_mint, slot.task,
                       native::args_frame, slot.args_frame, read_write) == success;
    }

    /*
     * Collects a finished child's exit status, or reports that it is still
     * running. Polls rather than blocks: process_wait is non-blocking by
     * design (see its ABI comment), so a caller loops on this the way every
     * other consumer here loops on a bounded ipc_receive timeout.
     */
    struct reaped {
        bool exited;
        word_t status;
    };

    [[nodiscard]] inline reaped reap(word_t index) noexcept {
        if (index >= dynamic_role_count)
            return {false, 0U};
        const child_slot slot = slot_for(index);
        word_t status = 0U;
        const word_t result =
            control_result1(status, abi::v1::control_operation::process_wait, slot.thread);
        if (result != static_cast<word_t>(error_t::success))
            return {false, 0U};
        return {true, status};
    }

    /*
     * Launches bin/argv-probe and checks the status it reports back.
     *
     * This is the only thing exercising the argument path, and it covers
     * all of it at once: a role drawn from the dynamic pool, an argument
     * block written by root, the frame minted into a child that maps and
     * parses it itself, the runtime dispatching to the POSIX entry rather
     * than the two-word one, and an exit status surviving back through
     * process_wait. 42 is the probe's success code; anything else is one of
     * its numbered failure modes (see programs/argv-probe/main.cc), so a
     * regression identifies which link broke.
     */
    /*
     * The pool's last role, reserved as bin/fork-probe's exec target rather
     * than used for spawning. role_image_bind is root-gated, so a forked
     * child cannot bind its own image; root binds this one ahead of time and
     * the child execs it by number. programs/fork-probe/main.cc hardcodes
     * the same value and must be kept in step.
     */
    inline constexpr word_t exec_probe_role = dynamic_role_base + dynamic_role_count - 1U;

    inline void release_child(word_t index) noexcept; // defined just below

    [[nodiscard]] inline bool verify_spawn_argv() noexcept {
        const char* const argv[] = {"argv-probe", "alpha", "beta"};
        const char* const envp[] = {"ZILCH=1"};
        if (!spawn(0U, "bin/argv-probe", argv, 3U, envp, 1U, 1U))
            return false;
        /*
         * Bounded poll rather than a blocking wait, matching every other
         * consumer here. Generous because the child is on another CPU and
         * has its own bounded retry to get through before it even runs.
         */
        reaped outcome{};
        for (word_t attempt = 0U; attempt < 2000000U && !outcome.exited; ++attempt)
            outcome = reap(0U);
        release_child(0U);
        return outcome.exited && outcome.status == 42U;
    }

    /*
     * Runs bin/fork-probe, which exercises fork and exec from inside a real
     * process and reports 44 only if both rounds passed. Its numbered
     * failure codes (see programs/fork-probe/main.cc) say which link broke.
     */
    [[nodiscard]] inline bool verify_fork_exec() noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const auto* page =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(earlyfs_scratch_address));
        constexpr usize_t directory_bound = 4096U;
        const auto target = platform::v1::earlyfs::find_span(page, directory_bound,
                                                             "bin/exec-probe");
        if (!target.found)
            return false;
        if (control(abi::v1::control_operation::role_image_bind, exec_probe_role,
                    static_cast<word_t>(target.offset),
                    static_cast<word_t>(target.size)) != success)
            return false;

        const char* const argv[] = {"fork-probe"};
        if (!spawn(1U, "bin/fork-probe", argv, 1U, nullptr, 0U, 2U))
            return false;
        /*
         * Yields a tick between polls instead of spinning: process_wait
         * takes the global authority lock, so hammering it starves the very
         * child being waited on. drain_fault_reports() is the yield -- it
         * blocks on a one-tick receive -- and doubles as fault reaping,
         * which matters here because the supervision thread that normally
         * does it is not spawned until after this check.
         */
        reaped outcome{};
        for (word_t attempt = 0U; attempt < 20000U && !outcome.exited; ++attempt) {
            outcome = reap(1U);
            if (!outcome.exited)
                drain_fault_reports(nullptr);
        }
        release_child(1U);
        return outcome.exited && outcome.status == 44U;
    }

    /*
     * Releases a reaped child's slot so the next spawn can reuse it. Must
     * follow reap(), not replace it: destroying the bundle discards the
     * status along with the thread.
     */
    inline void release_child(word_t index) noexcept {
        if (index >= dynamic_role_count)
            return;
        const child_slot slot = slot_for(index);
        (void)control(abi::v1::control_operation::process_destroy, slot.thread);
        (void)control(abi::v1::control_operation::frame_destroy, slot.args_frame);
    }

    /*
     * Baseline crash handling: drain root's own fault endpoint (see
     * root_fault_endpoint above) and reply terminate immediately for
     * whatever is waiting there, instead of leaving a crashed child to sit
     * blocked_fault for the kernel's own 500-tick fault_timeout_ticks
     * before it silently times out to state::terminated. This only makes
     * termination prompt -- it does not report the crash anywhere
     * (deferred: production userspace has no logging syscall today, and
     * OBS-010 forbids raw fault address/PC/syndrome in release logs even
     * if one existed, so a redaction-compliant visibility mechanism needs
     * its own separate design pass).
     *
     * ipc_receive() is a genuinely blocking primitive (unlike
     * notification_poll's instant memory read); a 1-tick bounded wait is
     * used to keep this from stalling the readiness-detection loop below
     * by more than one scheduler tick per iteration while a check is made.
     */
    inline constexpr word_t fault_poll_ticks = 1U;

    /*
     * The reply must always happen first, regardless of what follows: it's
     * what releases the crashed thread from blocked_fault. Restart is a
     * separate, subsequent decision. reply.sender is endpoint_badge()'s
     * value ((generation << 32) | (id + 1)) -- the inverse decode below
     * recovers the crashed thread's slot id, which is then correlated
     * against the shared role table to find which role (if any) just
     * crashed. state may be nullptr (see supervision_thread_entry()'s
     * comment) or the crashed id may not belong to any tracked role (e.g.
     * memory-server, which USR-034 doesn't cover -- see the restart-on-
     * fault plan's "Explicitly out of scope"); either way this just falls
     * back to today's terminate-only behavior.
     */
    inline void drain_fault_reports(supervisor_state* state) noexcept {
        const auto reply =
            ipc_receive(root_fault_endpoint, abi::v1::encode_timeout(fault_poll_ticks));
        if (reply.status != static_cast<word_t>(error_t::success))
            return;
        (void)ipc_reply(static_cast<word_t>(abi::v1::fault_disposition::terminate), 0U, 0U, 0U);
        if (state == nullptr)
            return;
        const u32 faulted_id = static_cast<u32>(reply.sender & 0xffffffffU) - 1U;
        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index) {
            if (state->roles[index].thread_id == faulted_id) {
                (void)restart_role(*state, index);
                return;
            }
        }
    }

    /*
     * Generic restart for process/device/console: destroy the crashed
     * bundle (waking anyone blocked_reply on it -- see
     * thread::release_pending_reply() in scheduler.hh), recreate it at the
     * same selectors, re-mint its own service endpoint (root's copy of the
     * endpoint object survives the old task's destruction untouched, so
     * every OTHER role's capability into it stays valid automatically --
     * only the fresh child needs a new mint into its own empty cspace), then
     * replay whatever role-specific extra minting that role's launch()
     * needed. domain additionally needs launch+load (not serve -- see
     * run_embedded_guest_loop(), which owns re-entering serve() from the
     * thread that isn't the one destroying/recreating the role).
     */
    [[nodiscard]] inline bool restart_role(supervisor_state& state, word_t index) noexcept {
        const word_t success = static_cast<word_t>(error_t::success);
        const word_t role = static_cast<word_t>(abi::v1::control_plane_role::process) + index;
        const auto policy = control_plane::policy_for(role);
        const word_t attempts = state.roles[index].restart_count;
        if (!control_plane::may_restart(policy, attempts))
            return false;

        const word_t selector = selector_base + index * selector_stride;
        if (control(abi::v1::control_operation::process_destroy, selector, selector + 1U,
                    selector + 2U) != success)
            return false;

        word_t new_id = 0U;
        if (control_result1(new_id, abi::v1::control_operation::process_create, index % 4U, role,
                            selector, selector + 1U, selector + 2U) != success)
            return false;

        const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                  static_cast<word_t>(abi::v1::CapabilityRight::write);
        if (control(abi::v1::control_operation::capability_mint, selector + 1U, service_endpoint,
                    endpoint_base + index, read_write, index + 1U) != success)
            return false;

        if (index == console_index && !mint_console_resources())
            return false;
#if CONFIG_GUEST_EMBEDDED_IMAGE
        if (index == domain_index) {
            if (!mint_embedded_guest_resources())
                return false;
            if (ipc_call(endpoint_base + domain_index,
                         static_cast<word_t>(abi::v1::control_plane_operation::launch), 0U)
                    .status != success)
                return false;
            if (ipc_call(endpoint_base + domain_index,
                         static_cast<word_t>(abi::v1::control_plane_operation::load), 0U)
                    .status != success)
                return false;
        }
#endif

        state.roles[index].thread_id = static_cast<u32>(new_id);
        state.roles[index].restart_count = static_cast<u32>(attempts + 1U);
        return true;
    }

#if CONFIG_GUEST_EMBEDDED_IMAGE
    /*
     * serve isn't a status query -- it IS the guest's execution loop
     * (domain/main.cc's serve handler is a for(;;) around domain.run()):
     * whichever thread calls it blocks for as long as the guest runs. After
     * a fault-triggered restart, restart_role() (run from the supervision
     * thread) already re-minted+launched+loaded a fresh domain-manager
     * instance; this thread -- the ORIGINAL one, which owns this whole
     * function and must stay the one that (re-)calls serve(), so the
     * supervision thread remains free to keep draining faults -- just needs
     * to notice and re-enter serve() on it.
     *
     * The restart-count comparison is what disambiguates "a fault-triggered
     * restart is in flight, wait then re-serve" from "the guest reached a
     * genuine non-fault terminal exit" (serve() returning with no restart
     * ever having been recorded): release_pending_reply() (scheduler.hh) is
     * what wakes this thread's blocked ipc_call in the crash case, by
     * construction always after restart_role() has destroyed the old task;
     * if restart_count never moves, nothing destroyed the role out from
     * under this call, so it was a real, non-fault exit and this stops.
     */
    [[nodiscard]] inline bool run_embedded_guest_loop(supervisor_state& state) noexcept {
        if (!create_embedded_guest_resources()) {
            report("guest: res failed\n");
            return false;
        }
        if (!mint_embedded_guest_resources()) {
            report("guest: mint failed\n");
            return false;
        }
        const capability_id_t domain_endpoint = endpoint_base + domain_index;
        const word_t success = static_cast<word_t>(error_t::success);
        if (ipc_call(domain_endpoint, static_cast<word_t>(abi::v1::control_plane_operation::launch),
                     0U)
                .status != success) {
            report("guest: launch failed\n");
            return false;
        }
        if (ipc_call(domain_endpoint, static_cast<word_t>(abi::v1::control_plane_operation::load),
                     0U)
                .status != success) {
            report("guest: load failed\n");
            return false;
        }
        report("guest: loaded, serving\n");

        u32 seen = state.roles[domain_index].restart_count;
        for (;;) {
            (void)ipc_call(domain_endpoint,
                           static_cast<word_t>(abi::v1::control_plane_operation::serve), 0U);
            constexpr u32 maximum_wait_rounds = 100000000U;
            bool restarted = false;
            for (u32 round = 0U; round < maximum_wait_rounds; ++round) {
                const u32 current = state.roles[domain_index].restart_count;
                if (current != seen) {
                    seen = current;
                    restarted = true;
                    break;
                }
            }
            if (!restarted)
                return false;
        }
    }
#endif

    [[nodiscard]] inline int supervise() noexcept {
        if (!bind_role_images())
            return 1;
        if (!create_supervisor_state())
            return 1;
        supervisor_state& state = *supervisor_state_ptr();
        if (control(abi::v1::control_operation::process_create, 1U, memory_role, memory_selector,
                    memory_selector + 1U,
                    memory_selector + 2U) != static_cast<word_t>(error_t::success))
            return 1;
        {
            const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
            if (control(abi::v1::control_operation::endpoint_create, memory_service_endpoint) !=
                static_cast<word_t>(error_t::success))
                return 1;
            if (control(abi::v1::control_operation::capability_mint, memory_selector + 1U,
                        service_endpoint, memory_service_endpoint,
                        read_write) != static_cast<word_t>(error_t::success))
                return 1;
        }
        if (control(abi::v1::control_operation::process_create, 2U, serial_role, serial_selector,
                    serial_selector + 1U,
                    serial_selector + 2U) != static_cast<word_t>(error_t::success))
            return 1;
        {
            const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
            if (control(abi::v1::control_operation::endpoint_create, serial_service_endpoint) !=
                static_cast<word_t>(error_t::success))
                return 1;
            if (control(abi::v1::control_operation::capability_mint, serial_selector + 1U,
                        service_endpoint, serial_service_endpoint,
                        read_write) != static_cast<word_t>(error_t::success))
                return 1;
            if (!create_serial_resources() || !mint_serial_resources())
                return 1;
            if (control(abi::v1::control_operation::endpoint_create, console_stdin_endpoint) !=
                static_cast<word_t>(error_t::success))
                return 1;
        }
        /*
         * Block driver, launched after serial-driver because its bring-up
         * diagnostics go out through serial-driver's service endpoint (see
         * mint_block_resources()) -- the endpoint object must exist and be
         * minted before this process can call into it.
         */
        if (control(abi::v1::control_operation::process_create, 3U, block_role, block_selector,
                    block_selector + 1U,
                    block_selector + 2U) != static_cast<word_t>(error_t::success))
            return 1;
        {
            const word_t read_write = static_cast<word_t>(abi::v1::CapabilityRight::read) |
                                      static_cast<word_t>(abi::v1::CapabilityRight::write);
            if (control(abi::v1::control_operation::endpoint_create, block_service_endpoint) !=
                static_cast<word_t>(error_t::success))
                return 1;
            if (control(abi::v1::control_operation::capability_mint, block_selector + 1U,
                        service_endpoint, block_service_endpoint,
                        read_write) != static_cast<word_t>(error_t::success))
                return 1;
            if (!create_block_resources() || !mint_block_resources())
                return 1;
        }
        for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index) {
            if (!launch(index, state))
                return 1;
            /*
             * Minted immediately after this specific launch(), not after
             * the whole loop: launch() schedules the new thread onto
             * index % 4U's CPU right away (process_create's IPI wakes
             * other CPUs), so the console-server could start running --
             * and call map_frame on the slot this mints into -- before
             * root reaches a later loop iteration or falls through the
             * loop entirely. Racing root's own mint against the
             * console-server's own map_frame is exactly the kind of
             * ordering bug this session already found the hard way once
             * (root_graph.hh's earlier earlyfs-binding fixes); minting
             * right here keeps it impossible by construction instead.
             */
            if (index == console_index && !mint_console_resources())
                return 1;
        }

        const word_t expected =
            ((1U << abi::v1::control_plane_role_count) - 1U) | abi::v1::memory_service_ready_badge |
            abi::v1::serial_service_ready_badge | abi::v1::block_service_ready_badge;
        word_t ready = 0U;
        bool console_verified = false;
        bool supervisor_spawned = false;
        for (;;) {
            // Once the supervision thread exists, it owns fault draining
            // (and thus restart) exclusively -- see the comment above
            // root_supervisor_role. Two threads racing to receive the same
            // fault and independently restart the same role is a hazard
            // this loop no longer needs to risk once that thread is up.
            if (!supervisor_spawned)
                drain_fault_reports(&state);
            word_t badges = 0U;
            const word_t status = control_result1(
                badges, abi::v1::control_operation::notification_poll, root_notification);
            if (status != static_cast<word_t>(error_t::success))
                return 2;
            if ((badges & (1U << 15U)) != 0U)
                return 3;
            ready |= badges;
            if ((ready & expected) == expected) {
                for (word_t index = 0U; index < abi::v1::control_plane_role_count; ++index)
                    if (!healthy(index))
                        return 4;
                /*
                 * Proves the console-server can genuinely drive the UART
                 * from userspace: this text reaches the real serial output
                 * without any kernel printk call in its path. Guarded so it
                 * fires exactly once -- without CONFIG_GUEST_EMBEDDED_IMAGE
                 * this whole branch re-runs every loop iteration once ready.
                 */
                if (!console_verified) {
                    if (console::write(endpoint_base + console_index, "console-server alive\n") !=
                        static_cast<word_t>(error_t::success))
                        return 6;
                    /*
                     * Same idea one layer down: prove the block service from
                     * a task other than the driver, exercising the IPC path
                     * and the shared payload frame rather than trusting the
                     * driver's internal self-check. Reported through the
                     * console that was just proven working.
                     */
                    const block_check block_state =
                        map_block_buffer() ? verify_block_service() : block_check::failed;
                    (void)console::write(
                        endpoint_base + console_index,
                        block_state == block_check::verified ? "block-service verified\n"
                        : block_state == block_check::absent ? "block-service absent\n"
                                                             : "block-service FAILED\n");
                    if (block_state == block_check::failed)
                        return 8;
                    report(verify_spawn_argv() ? "spawn-argv verified\n" : "spawn-argv FAILED\n");
                    report(verify_fork_exec() ? "fork-exec verified\n" : "fork-exec FAILED\n");
                    console_verified = true;
                }
                if (!supervisor_spawned) {
                    if (!spawn_supervision_thread()) {
                        (void)console::write(endpoint_base + console_index, "sup: spawn failed\n");
                        return 7;
                    }
                    supervisor_spawned = true;
                    /*
                     * Definitive "the whole graph is up" marker, printed
                     * once. Until this existed the last console output was
                     * the console check itself, so a boot that lost its
                     * supervision thread afterwards looked identical to a
                     * healthy one -- which is how that failure survived.
                     * tools/verification/smoke.sh asserts on this line.
                     */
                    report("graph ready\n");
#if CONFIG_FAULT_INJECTION
                    report(verify_restart_on_fault(state) ? "restart ok\n" : "restart FAILED\n");
#endif
                }
#if CONFIG_GUEST_EMBEDDED_IMAGE
                report("guest: starting\n");
                return run_embedded_guest_loop(state) ? 0 : 5;
#endif
            }
        }
    }
} // namespace sys::root_graph
