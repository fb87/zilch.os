#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::kernel::emergency
{
    inline constexpr u64 crash_magic = 0x5a494c4348525348ULL; // ZILCHRSH
    inline constexpr u16 record_format_version = 1U;
    inline constexpr u32 cpu_count = 4U;
    inline constexpr u32 records_per_cpu = 32U;

    enum class event : u32 {
        exception_entry = 1U,
        printk_contention = 2U,
        fatal_exception = 3U,
        stack_corruption = 4U,
        certification = 5U,
        irq = 6U,
        scheduler_switch = 7U,
        ipc = 8U,
        vm_exit = 9U,
        user_fault = 10U,
        /*
         * Device assignment and its withdrawal (OBS-008). Recorded here
         * rather than in the hypervisor audit ring because that ring is
         * VM-scoped, and a device in this system is assigned to a driver
         * PROCESS -- root mints its MMIO frame and interrupt line -- not to
         * a VM. Filing them against a vmid would mean inventing one.
         *
         * Uses append() rather than trace(), so these survive a release
         * build: who was handed a device, and when it was taken back, is
         * exactly what a post-mortem needs and is far too low-volume to
         * cost anything.
         */
        device_assign = 11U,
        device_revoke = 12U,
    };

    struct record {
        u64 sequence{};
        u16 version{};
        u16 reserved{};
        event kind{};
        cpu_id_t cpu{};
        u64 argument[5]{};
    };

    struct crash_record {
        u64 magic{};
        u64 sequence{};
        cpu_id_t cpu{};
        u32 level{};
        u64 vector{};
        u64 syndrome{};
        u64 fault_address{};
        u64 instruction_pointer{};
        u64 checksum{};
    };

    inline record buffers[cpu_count][records_per_cpu]{};
    inline volatile u64 next_sequence[cpu_count]{};
    inline crash_record preserved_crash __attribute__((section(".noinit")));

    /*
     * Records that a human is meant to read back asynchronously, as opposed
     * to the high-volume trace kinds that exist to be walked in a debugger
     * after the fact (OBS-003).
     */
    [[nodiscard]] inline bool reportable(event kind) noexcept {
        switch (kind) {
            case event::printk_contention:
            case event::fatal_exception:
            case event::stack_corruption:
            case event::user_fault:
            case event::device_assign:
            case event::device_revoke:
                return true;
            default:
                return false;
        }
    }

    /*
     * A dedicated ring for the reportable kinds, separate from the per-CPU
     * trace buffers above (OBS-003).
     *
     * The per-CPU rings cannot carry these. They are 32 records deep and, in
     * a CONFIG_TRACE build, every IRQ, IPC and context switch lands in them:
     * measured, they wrap several hundred times per second, which is orders
     * of magnitude faster than a drain running once per timer tick and
     * limited by console bandwidth can consume. Draining them directly was
     * tried and reported nothing but its own loss counter -- a printk
     * contention record was always already overwritten by trace traffic
     * before anything could format it.
     *
     * Mirroring the reportable kinds here costs a predictable-branch and a
     * handful of stores on paths that are rare by construction (a dropped
     * console line, a fault, a device handover), and gives the drain a ring
     * whose occupancy tracks diagnostics rather than trace volume. One ring
     * rather than one per CPU, so the sequence numbers order the records
     * against each other across CPUs.
     */
    inline constexpr u32 deferred_capacity = 64U;
    inline record deferred[deferred_capacity]{};
    inline volatile u64 deferred_sequence{};

    inline void append(event kind, u64 argument0 = 0U, u64 argument1 = 0U, u64 argument2 = 0U,
                       u64 argument3 = 0U, u64 argument4 = 0U) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu >= cpu_count)
            return;
        const u64 sequence = __atomic_fetch_add(&next_sequence[cpu], 1U, __ATOMIC_RELAXED) + 1U;
        record& destination = buffers[cpu][sequence % records_per_cpu];
        destination.version = record_format_version;
        destination.reserved = 0U;
        destination.kind = kind;
        destination.cpu = cpu;
        destination.argument[0] = argument0;
        destination.argument[1] = argument1;
        destination.argument[2] = argument2;
        destination.argument[3] = argument3;
        destination.argument[4] = argument4;
        __atomic_store_n(&destination.sequence, sequence, __ATOMIC_RELEASE);

        if (!reportable(kind))
            return;

        const u64 slot = __atomic_fetch_add(&deferred_sequence, 1U, __ATOMIC_RELAXED) + 1U;
        record& mirror = deferred[slot % deferred_capacity];
        mirror.version = record_format_version;
        mirror.reserved = 0U;
        mirror.kind = kind;
        mirror.cpu = cpu;
        mirror.argument[0] = argument0;
        mirror.argument[1] = argument1;
        mirror.argument[2] = argument2;
        mirror.argument[3] = argument3;
        mirror.argument[4] = argument4;
        __atomic_store_n(&mirror.sequence, slot, __ATOMIC_RELEASE);
    }

    inline void trace(event kind, u64 argument0 = 0U, u64 argument1 = 0U, u64 argument2 = 0U,
                      u64 argument3 = 0U, u64 argument4 = 0U) noexcept {
#if CONFIG_TRACE
        append(kind, argument0, argument1, argument2, argument3, argument4);
#else
        (void)kind;
        (void)argument0;
        (void)argument1;
        (void)argument2;
        (void)argument3;
        (void)argument4;
#endif
    }

    [[nodiscard]] inline u64 checksum(const crash_record& value) noexcept {
        return value.magic ^ value.sequence ^ value.cpu ^ value.level ^ value.vector ^
               value.syndrome ^ value.fault_address ^ value.instruction_pointer;
    }

    inline void preserve(u32 level, u64 vector, u64 syndrome, u64 fault_address,
                         u64 instruction_pointer) noexcept {
        crash_record value{};
        value.magic = crash_magic;
        value.sequence = preserved_crash.sequence + 1U;
        value.cpu = arch::cpu::current_id();
        value.level = level;
        value.vector = vector;
        value.syndrome = syndrome;
        value.fault_address = fault_address;
        value.instruction_pointer = instruction_pointer;
        value.checksum = checksum(value);
        preserved_crash = value;
        arch::cpu::full_barrier();
    }

    [[nodiscard]] inline bool crash_valid() noexcept {
        return preserved_crash.magic == crash_magic &&
               preserved_crash.checksum == checksum(preserved_crash);
    }

    [[nodiscard]] inline bool verify_ring() noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu >= cpu_count)
            return false;
        append(event::certification, 0xfeedfaceULL);
        const u64 sequence = __atomic_load_n(&next_sequence[cpu], __ATOMIC_ACQUIRE);
        const record& observed = buffers[cpu][sequence % records_per_cpu];
        return __atomic_load_n(&observed.sequence, __ATOMIC_ACQUIRE) == sequence &&
               observed.kind == event::certification && observed.cpu == cpu &&
               observed.version == record_format_version && observed.argument[0] == 0xfeedfaceULL;
    }
} // namespace sys::kernel::emergency
