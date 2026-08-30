#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/exception.hh>
#include <sys/types.hh>

#include <abi/sys/v1/hypervisor.hh>

namespace sys::arch::hypervisor
{
    inline constexpr u64 sctlr_el1_res1 = 0x30d00800ULL;
    inline constexpr u64 sctlr_el1_guest_control =
        (1ULL << 0U) | (1ULL << 1U) | (1ULL << 2U) | (1ULL << 3U) | (1ULL << 4U) | (1ULL << 5U) |
        (1ULL << 12U) | (1ULL << 14U) | (1ULL << 15U) | (1ULL << 19U) | (1ULL << 21U) |
        (1ULL << 24U) | (1ULL << 25U) | (1ULL << 26U) | (1ULL << 30U) | (1ULL << 31U);

    [[nodiscard]] inline constexpr u64 sanitize_guest_sctlr_el1(u64 requested) noexcept {
        return (requested & sctlr_el1_guest_control) | sctlr_el1_res1;
    }

    inline constexpr u64 guest_pstate_mask = 0xf00003c5ULL;
    inline constexpr u64 guest_pstate_required = 0x5ULL; // EL1h
    inline constexpr u64 guest_tcr_mask = 0x000000ffffffffffULL;
    inline constexpr u64 guest_cpacr_mask = 3ULL << 20U;
    inline constexpr u64 guest_cntkctl_mask = 0x3ffULL;

    [[nodiscard]] inline constexpr u64 sanitize_guest_pstate(u64 requested) noexcept {
        return (requested & guest_pstate_mask & ~0xfULL) | guest_pstate_required;
    }

    [[nodiscard]] inline constexpr u64 sanitize_guest_tcr_el1(u64 requested) noexcept {
        return requested & guest_tcr_mask;
    }

    [[nodiscard]] inline constexpr u64 sanitize_guest_cpacr_el1(u64 requested) noexcept {
        return requested & guest_cpacr_mask;
    }

    [[nodiscard]] inline constexpr u64 sanitize_guest_cntkctl_el1(u64 requested) noexcept {
        return requested & guest_cntkctl_mask;
    }

    [[nodiscard]] inline constexpr bool known_guest_hypercall(u64 value) noexcept {
        switch (static_cast<abi::v1::guest_hypercall>(value)) {
            case abi::v1::guest_hypercall::console_write:
            case abi::v1::guest_hypercall::time_query:
            case abi::v1::guest_hypercall::irq_acknowledge:
            case abi::v1::guest_hypercall::shutdown:
            case abi::v1::guest_hypercall::report:
            case abi::v1::guest_hypercall::diagnostic:
                return true;
        }
        return false;
    }

    [[nodiscard]] inline constexpr bool guest_abort_exception(u64 syndrome) noexcept {
        const u64 exception_class = (syndrome >> 26U) & 0x3fU;
        return exception_class == 0x20U || exception_class == 0x21U || exception_class == 0x24U ||
               exception_class == 0x25U;
    }

    [[nodiscard]] inline constexpr u64 guest_fault_ipa(u64 syndrome, u64 fault_address,
                                                       u64 hpfar) noexcept {
        if (!guest_abort_exception(syndrome))
            return 0U;
        constexpr u64 hpfar_fault_number_mask = 0x0000fffffffffff0ULL;
        return ((hpfar & hpfar_fault_number_mask) << 8U) | (fault_address & 0xfffU);
    }

    [[nodiscard]] inline constexpr u64 mmio_qualification(u64 syndrome, u64 ipa) noexcept {
        const u64 access_size = (syndrome >> 22U) & 3U;
        const u64 target_register = (syndrome >> 16U) & 0x1fU;
        const u64 sign_extend = (syndrome >> 21U) & 1U;
        const u64 write = (syndrome >> 6U) & 1U;
        return (ipa & 0x0000ffffffffffffULL) | (write << 63U) | (access_size << 61U) |
               (sign_extend << 60U) | (target_register << 55U);
    }

    [[nodiscard]] inline constexpr u32 trapped_system_register(u64 syndrome) noexcept {
        const u32 op0 = static_cast<u32>((syndrome >> 20U) & 3U);
        const u32 op2 = static_cast<u32>((syndrome >> 17U) & 7U);
        const u32 op1 = static_cast<u32>((syndrome >> 14U) & 7U);
        const u32 crn = static_cast<u32>((syndrome >> 10U) & 0xfU);
        const u32 crm = static_cast<u32>((syndrome >> 1U) & 0xfU);
        return (op0 << 14U) | (op1 << 11U) | (crn << 7U) | (crm << 3U) | op2;
    }
    inline constexpr bool available = true;
    inline constexpr bool active = true;
    inline constexpr u64 abi_signature = 0x5a494c4300000000ULL;
    inline constexpr u64 abi_version = 0x0000000000010000ULL;
    inline constexpr u64 test_cookie = 0x123456789abcdef0ULL;
    inline constexpr u32 maximum_cpu_count = 4U;

    enum class call_id : u64 {
        version = abi_signature | 0x01ULL,
        cpu_id = abi_signature | 0x02ULL,
        test = abi_signature | 0x03ULL,
        host_configure = abi_signature | 0x04ULL,
        stage2_invalidate = abi_signature | 0x05ULL,
        guest_run = abi_signature | 0x06ULL,
        vgic_maintenance = abi_signature | 0x07ULL,
    };

    struct guest_context_layout {
        u64 x[31];
        u64 pc;
        u64 pstate;
        u64 sp_el0;
        u64 sp_el1;
        u64 elr_el1;
        u64 spsr_el1;
        u64 sctlr_el1;
        u64 tcr_el1;
        u64 ttbr0_el1;
        u64 ttbr1_el1;
        u64 mair_el1;
        u64 vbar_el1;
        u64 tpidr_el0;
        u64 tpidr_el1;
        u64 cntv_ctl_el0;
        u64 cntv_cval_el0;
        u64 actlr_el1;
        u64 cpacr_el1;
        u64 contextidr_el1;
        u64 afsr0_el1;
        u64 afsr1_el1;
        u64 esr_el1;
        u64 far_el1;
        u64 par_el1;
        u64 cntkctl_el1;
        u64 ich_hcr_el2;
        u64 ich_vmcr_el2;
        u64 ich_ap0r0_el2;
        u64 ich_ap1r0_el2;
        u64 ich_lr_el2[16];
        u64 gic_enabled;
        u64 gic_pending;
        u64 gic_active;
        u64 gic_group1;
        u64 gic_edge;
        u8 gic_priority[64];
        u8 gic_pmr;
        u8 gic_bpr;
        u16 gic_last_iar;
        u32 gicd_ctlr;
        u32 gicr_waker;
        bool native_gic;
        u64 report_mask;
    };

    struct exit_layout {
        abi::v1::vm_exit_reason reason;
        u64 syndrome;
        u64 fault_address;
        u64 guest_pc;
        u64 qualification;
    };

    inline volatile u32 verified_cpu_mask = 0U;
    inline volatile u64 call_count[maximum_cpu_count]{};
    inline exception::frame_t host_frame[maximum_cpu_count]{};
    inline guest_context_layout* active_context[maximum_cpu_count]{};
    inline exit_layout* active_exit[maximum_cpu_count]{};
    inline bool guest_active[maximum_cpu_count]{};
    inline u64 saved_hcr[maximum_cpu_count]{};
    inline u64 saved_vttbr[maximum_cpu_count]{};
    inline u64 saved_vtcr[maximum_cpu_count]{};
    inline u64 saved_cntvoff[maximum_cpu_count]{};
    inline u64 requested_counter_offset[maximum_cpu_count]{};
    inline u64 guest_report_mask[maximum_cpu_count]{};
    inline bool guest_mmu_enable_armed[maximum_cpu_count]{};
    inline bool guest_irq_acknowledged[maximum_cpu_count]{};
    inline bool guest_uart_irq_pending[maximum_cpu_count]{};
    inline bool guest_uart_irq_reported[maximum_cpu_count]{};
    inline bool guest_uart_irq_injected[maximum_cpu_count]{};
    inline guest_context_layout saved_host_el1[maximum_cpu_count]{};
    inline u64 ich_vtr[maximum_cpu_count]{};

    inline void save_virtual_gic_state(guest_context_layout& state) noexcept {
        __asm__ volatile("mrs %0, ich_hcr_el2; mrs %1, ich_vmcr_el2; mrs %2, ich_ap0r0_el2; "
                         "mrs %3, ich_ap1r0_el2; mrs %4, ich_lr0_el2; mrs %5, ich_lr1_el2; "
                         "mrs %6, ich_lr2_el2; mrs %7, ich_lr3_el2"
                         : "=r"(state.ich_hcr_el2), "=r"(state.ich_vmcr_el2),
                           "=r"(state.ich_ap0r0_el2), "=r"(state.ich_ap1r0_el2),
                           "=r"(state.ich_lr_el2[0]), "=r"(state.ich_lr_el2[1]),
                           "=r"(state.ich_lr_el2[2]), "=r"(state.ich_lr_el2[3]));
        for (u32 index = 4U; index < 16U; ++index)
            state.ich_lr_el2[index] = 0U;
    }

    inline void load_virtual_gic_state(const guest_context_layout& state) noexcept {
        __asm__ volatile("msr ich_hcr_el2, %0; msr ich_vmcr_el2, %1; msr ich_ap0r0_el2, %2; "
                         "msr ich_ap1r0_el2, %3; msr ich_lr0_el2, %4; msr ich_lr1_el2, %5; "
                         "msr ich_lr2_el2, %6; msr ich_lr3_el2, %7; isb" ::"r"(state.ich_hcr_el2),
                         "r"(state.ich_vmcr_el2), "r"(state.ich_ap0r0_el2),
                         "r"(state.ich_ap1r0_el2), "r"(state.ich_lr_el2[0]),
                         "r"(state.ich_lr_el2[1]), "r"(state.ich_lr_el2[2]),
                         "r"(state.ich_lr_el2[3])
                         : "memory");
    }

    inline void save_el1_system_state(guest_context_layout& state) noexcept {
        __asm__ volatile("mrs %0, sp_el0; mrs %1, sp_el1; mrs %2, elr_el1; mrs %3, spsr_el1; "
                         "mrs %4, sctlr_el1; mrs %5, tcr_el1; mrs %6, ttbr0_el1; "
                         "mrs %7, ttbr1_el1; mrs %8, mair_el1; mrs %9, vbar_el1; "
                         "mrs %10, tpidr_el0; mrs %11, tpidr_el1; "
                         "mrs %12, cntv_ctl_el0; mrs %13, cntv_cval_el0; "
                         "mrs %14, actlr_el1; mrs %15, cpacr_el1; mrs %16, contextidr_el1; "
                         "mrs %17, afsr0_el1; mrs %18, afsr1_el1; mrs %19, esr_el1; "
                         "mrs %20, far_el1; mrs %21, par_el1; mrs %22, cntkctl_el1"
                         : "=r"(state.sp_el0), "=r"(state.sp_el1), "=r"(state.elr_el1),
                           "=r"(state.spsr_el1), "=r"(state.sctlr_el1), "=r"(state.tcr_el1),
                           "=r"(state.ttbr0_el1), "=r"(state.ttbr1_el1), "=r"(state.mair_el1),
                           "=r"(state.vbar_el1), "=r"(state.tpidr_el0), "=r"(state.tpidr_el1),
                           "=r"(state.cntv_ctl_el0), "=r"(state.cntv_cval_el0),
                           "=r"(state.actlr_el1), "=r"(state.cpacr_el1), "=r"(state.contextidr_el1),
                           "=r"(state.afsr0_el1), "=r"(state.afsr1_el1), "=r"(state.esr_el1),
                           "=r"(state.far_el1), "=r"(state.par_el1), "=r"(state.cntkctl_el1));
        save_virtual_gic_state(state);
    }

    inline void load_el1_system_state(const guest_context_layout& state) noexcept {
        __asm__ volatile(
            "msr sp_el0, %0; msr sp_el1, %1; msr elr_el1, %2; msr spsr_el1, %3; "
            "msr tcr_el1, %5; msr ttbr0_el1, %6; msr ttbr1_el1, %7; "
            "msr mair_el1, %8; msr vbar_el1, %9; "
            "msr tpidr_el0, %10; msr tpidr_el1, %11; "
            "msr cntv_ctl_el0, %12; msr cntv_cval_el0, %13; "
            "msr actlr_el1, %14; msr cpacr_el1, %15; msr contextidr_el1, %16; "
            "msr afsr0_el1, %17; msr afsr1_el1, %18; msr esr_el1, %19; "
            "msr far_el1, %20; msr par_el1, %21; msr cntkctl_el1, %22; "
            "msr sctlr_el1, %4; isb" ::"r"(state.sp_el0),
            "r"(state.sp_el1), "r"(state.elr_el1), "r"(state.spsr_el1), "r"(state.sctlr_el1),
            "r"(state.tcr_el1), "r"(state.ttbr0_el1), "r"(state.ttbr1_el1), "r"(state.mair_el1),
            "r"(state.vbar_el1), "r"(state.tpidr_el0), "r"(state.tpidr_el1),
            "r"(state.cntv_ctl_el0), "r"(state.cntv_cval_el0), "r"(state.actlr_el1),
            "r"(sanitize_guest_cpacr_el1(state.cpacr_el1)), "r"(state.contextidr_el1),
            "r"(state.afsr0_el1), "r"(state.afsr1_el1), "r"(state.esr_el1), "r"(state.far_el1),
            "r"(state.par_el1), "r"(sanitize_guest_cntkctl_el1(state.cntkctl_el1))
            : "memory");
        load_virtual_gic_state(state);
    }

    inline void copy_frame(exception::frame_t& destination,
                           const exception::frame_t& source) noexcept {
        for (u32 index = 0U; index < 31U; ++index)
            destination.x[index] = source.x[index];
        destination.vector = source.vector;
        destination.stack_pointer = source.stack_pointer;
        destination.instruction_pointer = source.instruction_pointer;
        destination.status = source.status;
    }

    inline void frame_from_context(exception::frame_t& frame,
                                   const guest_context_layout& context) noexcept {
        for (u32 index = 0U; index < 31U; ++index)
            frame.x[index] = context.x[index];
        frame.stack_pointer = context.sp_el0;
        frame.instruction_pointer = context.pc;
        frame.status = context.pstate;
    }

    inline void context_from_frame(guest_context_layout& context,
                                   const exception::frame_t& frame) noexcept {
        for (u32 index = 0U; index < 31U; ++index)
            context.x[index] = frame.x[index];
        context.sp_el0 = frame.stack_pointer;
        context.pc = frame.instruction_pointer;
        context.pstate = frame.status;
    }

    inline constexpr u64 gicd_base = 0x08000000ULL;
    inline constexpr u64 gicr_base = 0x080a0000ULL;
    inline constexpr u16 gic_spurious_intid = 1023U;

    [[nodiscard]] inline u64 native_gic_deliverable(const guest_context_layout& state) noexcept {
        if ((state.gicd_ctlr & 2U) == 0U)
            return 0U;
        u64 candidates = state.gic_pending & state.gic_enabled & state.gic_group1;
        for (u32 intid = 0U; intid < 64U; ++intid) {
            const u64 bit = 1ULL << intid;
            if ((candidates & bit) != 0U && state.gic_priority[intid] >= state.gic_pmr)
                candidates &= ~bit;
        }
        return candidates;
    }

    inline void update_native_gic_vi(const guest_context_layout& state) noexcept {
        u64 hcr = 0U;
        __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
        if (native_gic_deliverable(state) != 0U)
            hcr |= 1ULL << 7U;
        else
            hcr &= ~(1ULL << 7U);
        __asm__ volatile("msr hcr_el2, %0; isb" : : "r"(hcr) : "memory");
    }

    inline void route_guest_uart_irq() noexcept {
        u64 mpidr = 0U;
        __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
        constexpr u64 affinity_mask = 0xff00ffffffULL;
        auto* route = reinterpret_cast<volatile u64*>(gicd_base + 0x6000U + 33U * 8U);
        *route = mpidr & affinity_mask;
        __asm__ volatile("dsb sy; isb" ::: "memory");
    }

    inline void enable_guest_uart_irq() noexcept {
        auto* priority = reinterpret_cast<volatile u8*>(gicd_base + 0x400U + 33U);
        auto* group = reinterpret_cast<volatile u32*>(gicd_base + 0x80U + 4U);
        auto* enable = reinterpret_cast<volatile u32*>(gicd_base + 0x100U + 4U);
        *priority = 0x80U;
        *group |= 1U << 1U;
        *enable = 1U << 1U;
        __asm__ volatile("dsb sy; isb" ::: "memory");
    }

    /*
     * The physical UART SPI is level-triggered and stays asserted until the
     * guest drains the FIFO. Once taken, mask it at the real distributor so
     * it cannot re-trap EL2 before the guest's virtual ISR gets to run;
     * re-enable happens only when the guest EOIs/deactivates virtual IRQ 33
     * (see emulate_native_gic_system_register). Without this, a level IRQ
     * that arrives while the guest has interrupts masked re-fires on every
     * ERET and the guest never advances past that instruction.
     */
    inline void disable_guest_uart_irq() noexcept {
        auto* disable = reinterpret_cast<volatile u32*>(gicd_base + 0x180U + 4U);
        *disable = 1U << 1U;
        __asm__ volatile("dsb sy; isb" ::: "memory");
    }

    [[nodiscard]] inline bool handle_guest_uart_irq() noexcept {
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count)
            return false;
        disable_guest_uart_irq();
        __atomic_store_n(&guest_uart_irq_pending[id], true, __ATOMIC_RELEASE);
        if (!guest_active[id] || active_context[id] == nullptr || !active_context[id]->native_gic)
            return true;
        __atomic_store_n(&guest_uart_irq_pending[id], false, __ATOMIC_RELEASE);
        constexpr u64 uart_bit = 1ULL << 33U;
        active_context[id]->gicd_ctlr |= 2U;
        active_context[id]->gic_group1 |= uart_bit;
        active_context[id]->gic_enabled |= uart_bit;
        active_context[id]->gic_priority[33U] = 0x80U;
        active_context[id]->gic_pending |= uart_bit;
        update_native_gic_vi(*active_context[id]);
        return true;
    }

    [[nodiscard]] inline bool handle_guest_virtual_timer_irq() noexcept {
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count || !guest_active[id] || active_context[id] == nullptr ||
            !active_context[id]->native_gic)
            return false;
        u64 control = 0U;
        __asm__ volatile("mrs %0, cntv_ctl_el0" : "=r"(control));
        control |= 1U << 1U;
        __asm__ volatile("msr cntv_ctl_el0, %0; isb" : : "r"(control) : "memory");
        active_context[id]->gic_pending |= 1ULL << 27U;
        update_native_gic_vi(*active_context[id]);
        return true;
    }

    [[nodiscard]] inline u32 native_gic_read(const guest_context_layout& state, bool redistributor,
                                              u32 offset) noexcept {
        if (redistributor && offset < 0x10000U) {
            if (offset == 0x8U)
                return 1U << 4U; // Last: this VM exposes exactly one redistributor.
            if (offset == 0x14U)
                return state.gicr_waker & 2U;
            return 0U;
        }
        const u32 local_offset = redistributor ? offset - 0x10000U : offset;
        const auto bitmap = [redistributor, local_offset](u64 value, u32 base) {
            const u32 register_index = (local_offset - base) / 4U;
            const u32 shift = redistributor ? 0U : register_index * 32U;
            return shift >= 64U ? 0U : static_cast<u32>(value >> shift);
        };
        if (!redistributor && local_offset == 0U)
            return state.gicd_ctlr;
        if (!redistributor && local_offset == 0x4U)
            return 1U; // 64 INTIDs (ITLinesNumber == 1), no affinity routing/ITS.
        if (local_offset >= 0x80U && local_offset < 0x88U)
            return bitmap(state.gic_group1, 0x80U);
        if ((local_offset >= 0x100U && local_offset < 0x108U) ||
            (local_offset >= 0x180U && local_offset < 0x188U))
            return bitmap(state.gic_enabled, local_offset < 0x180U ? 0x100U : 0x180U);
        if ((local_offset >= 0x200U && local_offset < 0x208U) ||
            (local_offset >= 0x280U && local_offset < 0x288U))
            return bitmap(state.gic_pending, local_offset < 0x280U ? 0x200U : 0x280U);
        if ((local_offset >= 0x300U && local_offset < 0x308U) ||
            (local_offset >= 0x380U && local_offset < 0x388U))
            return bitmap(state.gic_active, local_offset < 0x380U ? 0x300U : 0x380U);
        if (local_offset >= 0x400U && local_offset < 0x440U) {
            u32 value = 0U;
            const u32 first = local_offset - 0x400U;
            for (u32 byte = 0U; byte < 4U && first + byte < 64U; ++byte)
                value |= static_cast<u32>(state.gic_priority[first + byte]) << (byte * 8U);
            return value;
        }
        if (local_offset >= 0xc00U && local_offset < 0xc08U) {
            u32 value = 0U;
            const u32 first = (local_offset - 0xc00U) * 16U;
            for (u32 bit = 0U; bit < 16U && first + bit < 64U; ++bit)
                if ((state.gic_edge & (1ULL << (first + bit))) != 0U)
                    value |= 2U << (bit * 2U);
            return value;
        }
        return 0U;
    }

    inline void native_gic_write(guest_context_layout& state, bool redistributor, u32 offset,
                                 u32 value) noexcept {
        if (redistributor && offset < 0x10000U) {
            if (offset == 0x14U)
                state.gicr_waker = value & 2U;
            return;
        }
        const u32 local_offset = redistributor ? offset - 0x10000U : offset;
        const auto bitmap_shift = [redistributor, local_offset](u32 base) {
            return redistributor ? 0U : ((local_offset - base) / 4U) * 32U;
        };
        const bool bitmap_register = (local_offset >= 0x80U && local_offset < 0x88U) ||
                                     (local_offset >= 0x100U && local_offset < 0x108U) ||
                                     (local_offset >= 0x180U && local_offset < 0x188U) ||
                                     (local_offset >= 0x200U && local_offset < 0x208U) ||
                                     (local_offset >= 0x280U && local_offset < 0x288U) ||
                                     (local_offset >= 0x300U && local_offset < 0x308U) ||
                                     (local_offset >= 0x380U && local_offset < 0x388U);
        const u32 base = local_offset & ~7U;
        const u32 shift = bitmap_register ? bitmap_shift(base) : 0U;
        const u64 bits = static_cast<u64>(value) << shift;
        const u64 valid = shift == 0U ? 0xffffffffULL : 0xffffffff00000000ULL;
        if (!redistributor && local_offset == 0U) {
            state.gicd_ctlr = value & 3U;
        } else if (local_offset >= 0x80U && local_offset < 0x88U) {
            state.gic_group1 = (state.gic_group1 & ~valid) | bits;
        } else if (local_offset >= 0x100U && local_offset < 0x108U) {
            state.gic_enabled |= bits;
#if CONFIG_GUEST_ZEPHYR
            if ((bits & (1ULL << 33U)) != 0U)
                enable_guest_uart_irq();
#endif
        } else if (local_offset >= 0x180U && local_offset < 0x188U) {
            state.gic_enabled &= ~bits;
        } else if (local_offset >= 0x200U && local_offset < 0x208U) {
            state.gic_pending |= bits;
        } else if (local_offset >= 0x280U && local_offset < 0x288U) {
            state.gic_pending &= ~bits;
        } else if (local_offset >= 0x300U && local_offset < 0x308U) {
            state.gic_active |= bits;
        } else if (local_offset >= 0x380U && local_offset < 0x388U) {
            state.gic_active &= ~bits;
        } else if (local_offset >= 0x400U && local_offset < 0x440U) {
            const u32 first = local_offset - 0x400U;
            for (u32 byte = 0U; byte < 4U && first + byte < 64U; ++byte)
                state.gic_priority[first + byte] = static_cast<u8>(value >> (byte * 8U));
        } else if (local_offset >= 0xc00U && local_offset < 0xc08U) {
            const u32 first = (local_offset - 0xc00U) * 16U;
            for (u32 bit = 0U; bit < 16U && first + bit < 64U; ++bit) {
                const u64 mask = 1ULL << (first + bit);
                if ((value & (2U << (bit * 2U))) != 0U)
                    state.gic_edge |= mask;
                else
                    state.gic_edge &= ~mask;
            }
        }
    }

    [[nodiscard]] inline bool emulate_native_gic_mmio(exception::frame_t& frame, u64 syndrome,
                                                       u64 ipa) noexcept {
        const bool distributor = ipa >= gicd_base && ipa < gicd_base + 0x10000ULL;
        const bool redistributor = ipa >= gicr_base && ipa < gicr_base + 0x20000ULL;
        const u32 bytes = 1U << ((syndrome >> 22U) & 3U);
        const u32 offset = static_cast<u32>(redistributor ? ipa - gicr_base : ipa - gicd_base);
        if ((!distributor && !redistributor) || bytes > 8U || (offset & (bytes - 1U)) != 0U ||
            (bytes <= 4U && (offset & 3U) + bytes > 4U))
            return false;
        const cpu_id_t id = cpu::current_id();
        auto* state = active_context[id];
        if (state == nullptr)
            return false;
        state->native_gic = true;
        const u32 target = static_cast<u32>((syndrome >> 16U) & 0x1fU);
        if (bytes == 8U) {
            if (!redistributor && offset >= 0x6100U && offset < 0x6300U) {
                if (((syndrome >> 6U) & 1U) == 0U && target != 31U)
                    frame.x[target] = 0U;
                frame.instruction_pointer += 4U;
                return true;
            }
            if (!redistributor || offset != 0x8U || ((syndrome >> 6U) & 1U) != 0U)
                return false;
            if (target != 31U) {
                u64 mpidr = 0U;
                __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
                const u64 affinity = (mpidr & 0x00ffffffU) | ((mpidr >> 8U) & 0xff000000U);
                frame.x[target] = (affinity << 32U) | (1U << 4U);
            }
            frame.instruction_pointer += 4U;
            return true;
        }
        const u32 aligned_offset = offset & ~3U;
        const u32 shift = (offset & 3U) * 8U;
        const u32 mask = bytes == 4U ? 0xffffffffU : ((1U << (bytes * 8U)) - 1U);
        if (((syndrome >> 6U) & 1U) != 0U) {
            const u32 previous = native_gic_read(*state, redistributor, aligned_offset);
            const u32 operand = target == 31U ? 0U : static_cast<u32>(frame.x[target]);
            const u32 local_offset = redistributor ? aligned_offset - 0x10000U : aligned_offset;
            const bool command_register =
                (local_offset >= 0x100U && local_offset < 0x108U) ||
                (local_offset >= 0x180U && local_offset < 0x188U) ||
                (local_offset >= 0x200U && local_offset < 0x208U) ||
                (local_offset >= 0x280U && local_offset < 0x288U) ||
                (local_offset >= 0x300U && local_offset < 0x308U) ||
                (local_offset >= 0x380U && local_offset < 0x388U);
            const u32 write_value = (operand & mask) << shift;
            native_gic_write(*state, redistributor, aligned_offset,
                             command_register ? write_value
                                              : (previous & ~(mask << shift)) | write_value);
        } else if (target != 31U) {
            const u32 value = (native_gic_read(*state, redistributor, aligned_offset) >> shift) & mask;
            frame.x[target] = value;
        }
        __asm__ volatile("msr ich_hcr_el2, %0; isb"
                         :
                         : "r"((1ULL << 12U) | (1ULL << 10U) | 1U)
                         : "memory");
        update_native_gic_vi(*state);
        frame.instruction_pointer += 4U;
        return true;
    }

    [[nodiscard]] inline bool emulate_native_gic_system_register(exception::frame_t& frame,
                                                                   u64 syndrome) noexcept {
        const cpu_id_t id = cpu::current_id();
        auto* state = active_context[id];
        if (state == nullptr || !state->native_gic)
            return false;
        constexpr u32 icc_pmr_el1 = 0xc230U;
        constexpr u32 icc_iar1_el1 = 0xc660U;
        constexpr u32 icc_eoir1_el1 = 0xc661U;
        constexpr u32 icc_dir_el1 = 0xc659U;
        constexpr u32 icc_bpr1_el1 = 0xc663U;
        constexpr u32 icc_ctlr_el1 = 0xc664U;
        constexpr u32 icc_sre_el1 = 0xc665U;
        constexpr u32 icc_igrpen1_el1 = 0xc667U;
        const u32 encoding = trapped_system_register(syndrome);
        const u32 target = static_cast<u32>((syndrome >> 5U) & 0x1fU);
        const bool read = (syndrome & 1U) != 0U;
        u64 value = target == 31U ? 0U : frame.x[target];
        bool handled = true;
        if (read) {
            if (encoding == icc_pmr_el1)
                value = state->gic_pmr;
            else if (encoding == icc_iar1_el1) {
                const u64 candidates = native_gic_deliverable(*state);
                value = gic_spurious_intid;
                if (candidates != 0U) {
                    const u16 intid = static_cast<u16>(__builtin_ctzll(candidates));
                    state->gic_pending &= ~(1ULL << intid);
                    state->gic_active |= 1ULL << intid;
                    state->gic_last_iar = intid;
                    if (intid == 33U)
                        __atomic_store_n(&guest_uart_irq_injected[id], true, __ATOMIC_RELEASE);
                    value = intid;
                }
            } else if (encoding == icc_bpr1_el1)
                value = state->gic_bpr;
            else if (encoding == icc_ctlr_el1)
                value = 0U;
            else if (encoding == icc_sre_el1)
                value = 1U;
            else if (encoding == icc_igrpen1_el1)
                value = (state->gicd_ctlr >> 1U) & 1U;
            else
                handled = false;
            if (handled && target != 31U)
                frame.x[target] = value;
        } else {
            if (encoding == icc_pmr_el1)
                state->gic_pmr = static_cast<u8>(value);
            else if (encoding == icc_bpr1_el1)
                state->gic_bpr = static_cast<u8>(value & 7U);
            else if (encoding == icc_eoir1_el1 || encoding == icc_dir_el1) {
                const u16 intid = static_cast<u16>(value);
                if (intid < 64U) {
                    state->gic_active &= ~(1ULL << intid);
                    if (intid == 27U) {
                        u64 control = 0U;
                        __asm__ volatile("mrs %0, cntv_ctl_el0" : "=r"(control));
                        control &= ~(1U << 1U);
                        __asm__ volatile("msr cntv_ctl_el0, %0; isb"
                                         :
                                         : "r"(control)
                                         : "memory");
                    }
#if CONFIG_GUEST_ZEPHYR
                    if (intid == 33U)
                        enable_guest_uart_irq();
#endif
                }
            } else if (encoding == icc_sre_el1) {
                // System-register interface is always enabled for this virtual CPU.
            } else if (encoding == icc_igrpen1_el1) {
                state->gicd_ctlr =
                    (state->gicd_ctlr & ~2U) | static_cast<u32>((value & 1U) << 1U);
            } else
                handled = false;
        }
        if (!handled)
            return false;
        update_native_gic_vi(*state);
        frame.instruction_pointer += 4U;
        return true;
    }

    [[nodiscard]] inline u64 call(call_id function, u64 argument0 = 0U, u64 argument1 = 0U,
                                  u64 argument2 = 0U, u64 argument3 = 0U) noexcept {
        register u64 x0 __asm__("x0") = static_cast<u64>(function);
        register u64 x1 __asm__("x1") = argument0;
        register u64 x2 __asm__("x2") = argument1;
        register u64 x3 __asm__("x3") = argument2;
        register u64 x4 __asm__("x4") = argument3;
        __asm__ volatile("hvc #0"
                         : "+r"(x0)
                         : "r"(x1), "r"(x2), "r"(x3), "r"(x4)
                         : "x5", "x6", "x7", "memory");
        return x0;
    }

    [[nodiscard]] inline error_t configure_host() noexcept {
        return call(call_id::host_configure) == 0U ? error_t::success : error_t::unsupported;
    }

    [[nodiscard]] inline bool virtual_gic_hardware_available() noexcept {
        const cpu_id_t id = cpu::current_id();
        return id < maximum_cpu_count && __atomic_load_n(&ich_vtr[id], __ATOMIC_ACQUIRE) != 0U;
    }

    [[nodiscard]] inline bool consume_virtual_irq_acknowledgement() noexcept {
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count)
            return false;
        return __atomic_exchange_n(&guest_irq_acknowledged[id], false, __ATOMIC_ACQ_REL);
    }

    [[nodiscard]] inline u64 virtual_gic_maintenance_status() noexcept {
        return call(call_id::vgic_maintenance);
    }

    inline void invalidate_stage2(u16 vmid) noexcept {
        (void)call(call_id::stage2_invalidate, vmid);
    }

    template <typename Context, typename Exit>
    [[nodiscard]] inline error_t run_guest(u16 vmid, paddr_t stage2_root, Context& context,
                                           Exit& exit, u64 counter_offset,
                                           bool virtual_irq_pending = false) noexcept {
        static_assert(sizeof(Context) == sizeof(guest_context_layout));
        static_assert(sizeof(Exit) == sizeof(exit_layout));
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count)
            return error_t::invalid_argument;
        requested_counter_offset[id] = counter_offset;
        const u64 run_flags = static_cast<u64>(vmid) | (virtual_irq_pending ? (1ULL << 16U) : 0U);
        const u64 result = call(call_id::guest_run, run_flags, stage2_root,
                                reinterpret_cast<u64>(&context), reinterpret_cast<u64>(&exit));
        return static_cast<error_t>(static_cast<s64>(result));
    }

    [[nodiscard]] inline error_t initialize() noexcept {
        return error_t::success;
    }

    [[nodiscard]] inline bool initialize_cpu() noexcept {
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count)
            return false;
        if (call(call_id::version) != abi_version)
            return false;
        if (call(call_id::cpu_id) != static_cast<u64>(id))
            return false;
        if (call(call_id::test, test_cookie) != test_cookie)
            return false;
        if (call(call_id::host_configure) != 0U)
            return false;
        __atomic_fetch_or(&verified_cpu_mask, 1U << id, __ATOMIC_RELEASE);
        return true;
    }

    [[nodiscard]] inline u32 verified_count() noexcept {
        return static_cast<u32>(
            __builtin_popcount(__atomic_load_n(&verified_cpu_mask, __ATOMIC_ACQUIRE)));
    }

    inline void console_putc(char value) noexcept {
        auto* uart = reinterpret_cast<volatile u32*>(0x09000000ULL);
        while ((uart[6] & (1U << 5U)) != 0U) {
        }
        uart[0] = static_cast<u32>(static_cast<unsigned char>(value));
    }

    inline void console_puts(const char* text) noexcept {
        if (text == nullptr)
            return;
        while (*text != '\0')
            console_putc(*text++);
    }

    inline void console_hex(u64 value) noexcept {
        console_puts("0x");
        bool started = false;
        for (s32 shift = 60; shift >= 0; shift -= 4) {
            const u8 digit = static_cast<u8>((value >> static_cast<u32>(shift)) & 0xfU);
            if (digit != 0U || started || shift == 0) {
                const char encoded =
                    static_cast<char>(digit < 10U ? static_cast<u8>('0') + digit
                                                  : static_cast<u8>('a') + digit - 10U);
                console_putc(encoded);
                started = true;
            }
        }
    }

    inline void console_field(const char* name, u64 value) noexcept {
        console_putc(' ');
        console_puts(name);
        console_putc('=');
        console_hex(value);
    }

    inline void console_stage2_walk(const char* name, u64 ipa, u64 vttbr) noexcept {
        constexpr u64 address_mask = 0x0000fffffffff000ULL;
        const u64 root_address = vttbr & address_mask;
        const auto* l1 = reinterpret_cast<const volatile u64*>(root_address);
        const u64 l1_index = (ipa >> 30U) & 0x1ffU;
        const u64 l1_descriptor = l1[l1_index];
        u64 l2_descriptor = 0U;
        u64 l3_descriptor = 0U;
        if ((l1_descriptor & 0x3U) == 0x3U) {
            const auto* l2 = reinterpret_cast<const volatile u64*>(l1_descriptor & address_mask);
            const u64 l2_index = (ipa >> 21U) & 0x1ffU;
            l2_descriptor = l2[l2_index];
            if ((l2_descriptor & 0x3U) == 0x3U) {
                const auto* l3 =
                    reinterpret_cast<const volatile u64*>(l2_descriptor & address_mask);
                const u64 l3_index = (ipa >> 12U) & 0x1ffU;
                l3_descriptor = l3[l3_index];
            }
        }
        console_puts("\n[HV-S2] object=");
        console_puts(name);
        console_field("ipa", ipa);
        console_field("l1", l1_descriptor);
        console_field("l2", l2_descriptor);
        console_field("l3", l3_descriptor);
        console_putc('\n');
    }

    inline void diagnose_guest_mmu(const exception::frame_t& frame) noexcept {
        u64 sctlr = 0U;
        u64 tcr = 0U;
        u64 ttbr0 = 0U;
        u64 ttbr1 = 0U;
        u64 mair = 0U;
        u64 vbar = 0U;
        u64 par = 0U;
        u64 hcr = 0U;
        u64 vtcr = 0U;
        u64 vttbr = 0U;
        __asm__ volatile("mrs %0, sctlr_el1; mrs %1, tcr_el1; mrs %2, ttbr0_el1; "
                         "mrs %3, ttbr1_el1; mrs %4, mair_el1; mrs %5, vbar_el1; "
                         "mrs %6, par_el1; mrs %7, hcr_el2; mrs %8, vtcr_el2; "
                         "mrs %9, vttbr_el2"
                         : "=r"(sctlr), "=r"(tcr), "=r"(ttbr0), "=r"(ttbr1), "=r"(mair), "=r"(vbar),
                           "=r"(par), "=r"(hcr), "=r"(vtcr), "=r"(vttbr));

        const u64 phase = frame.x[1];
        console_puts("\n[HV-MMU] phase=");
        if (phase == 1U)
            console_puts("pre-enable");
        else if (phase == 2U)
            console_puts("post-enable");
        else if (phase == 3U)
            console_puts("guest-fault");
        else
            console_hex(phase);
        console_field("pc", frame.instruction_pointer);
        console_field("sp", frame.x[5]);
        console_field("sctlr", sctlr);
        console_field("tcr", tcr);
        console_field("ttbr0", ttbr0);
        console_field("ttbr1", ttbr1);
        console_field("mair", mair);
        console_field("vbar", vbar);
        console_field("par", par);
        console_field("hcr", hcr);
        console_field("vtcr", vtcr);
        console_field("vttbr", vttbr);
        if (phase == 1U) {
            console_field("l1", frame.x[2]);
            console_field("l2", frame.x[3]);
            console_field("target", frame.x[4]);
            console_field("guest_sp", frame.x[5]);
            console_field("probe_par", frame.x[6]);
            console_field("pre_sctlr", frame.x[7]);
            console_stage2_walk("vector", 0x800U, vttbr);
            console_stage2_walk("current", frame.instruction_pointer, vttbr);
            console_stage2_walk("target", frame.x[4], vttbr);
            console_stage2_walk("l1-table", ttbr0, vttbr);
            console_stage2_walk("l2-table", frame.x[2] & 0x0000fffffffff000ULL, vttbr);
        } else if (phase == 2U) {
            console_field("post_sctlr", frame.x[2]);
            console_field("post_tcr", frame.x[3]);
            console_field("post_ttbr0", frame.x[4]);
            console_field("target", frame.x[5]);
            console_field("guest_sp", frame.x[6]);
            console_field("post_par", frame.x[7]);
        } else if (phase == 3U) {
            console_field("esr", frame.x[2]);
            console_field("far", frame.x[3]);
            console_field("elr", frame.x[4]);
            console_field("spsr", frame.x[5]);
            console_field("guest_sp", frame.x[6]);
            console_field("fault_par", frame.x[7]);
        }
        console_putc('\n');
    }

    inline void complete_guest_exit(exception::frame_t& frame, u64 syndrome,
                                    abi::v1::vm_exit_reason reason) noexcept {
        const cpu_id_t id = cpu::current_id();
        auto* context = active_context[id];
        auto* exit = active_exit[id];
        if (context != nullptr) {
            context_from_frame(*context, frame);
            save_el1_system_state(*context);
        }
        if (exit != nullptr) {
            exit->reason = reason;
            exit->syndrome = syndrome;
            exit->fault_address = exception::fault_address(2U);
            exit->guest_pc = frame.instruction_pointer;
            u64 hpfar = 0U;
            __asm__ volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
            exit->qualification = guest_fault_ipa(syndrome, exit->fault_address, hpfar);
        }
        load_el1_system_state(saved_host_el1[id]);
        __asm__ volatile("msr hcr_el2, %0; msr vttbr_el2, %1; msr vtcr_el2, %2; "
                         "msr cntvoff_el2, %3; isb" ::"r"(saved_hcr[id]),
                         "r"(saved_vttbr[id]), "r"(saved_vtcr[id]), "r"(saved_cntvoff[id])
                         : "memory");
        guest_active[id] = false;
        active_context[id] = nullptr;
        active_exit[id] = nullptr;
        copy_frame(frame, host_frame[id]);
        frame.x[0] = static_cast<u64>(static_cast<s64>(error_t::success));
    }

    [[nodiscard]] inline bool dispatch(exception::frame_t& frame, u64 syndrome) noexcept {
        constexpr u64 exception_class_mask = 0x3fULL;
        constexpr u64 hvc64_exception_class = 0x16ULL;
        const u64 exception_class = (syndrome >> 26U) & exception_class_mask;
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count)
            return false;

        if (guest_active[id]) {
            if (exception_class == hvc64_exception_class) {
                const u64 hypercall_number = frame.x[0];
                const auto hypercall = static_cast<abi::v1::guest_hypercall>(frame.x[0]);
                switch (hypercall) {
                    case abi::v1::guest_hypercall::console_write:
                        console_putc(static_cast<char>(frame.x[1] & 0xffU));
                        frame.x[0] = 0U;
                        return true;
                    case abi::v1::guest_hypercall::time_query: {
                        u64 counter = 0U;
                        __asm__ volatile("mrs %0, cntpct_el0" : "=r"(counter));
                        frame.x[0] = counter;
                        return true;
                    }
                    case abi::v1::guest_hypercall::irq_acknowledge: {
                        u64 hcr = 0U;
                        __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
                        hcr &= ~(1ULL << 7U);
                        __asm__ volatile("msr hcr_el2, %0; msr ich_lr0_el2, xzr; isb" ::"r"(hcr)
                                         : "memory");
                        __atomic_store_n(&guest_irq_acknowledged[id], true, __ATOMIC_RELEASE);
                        frame.x[0] = 0U;
                        return true;
                    }
                    case abi::v1::guest_hypercall::report:
                        if (active_context[id] != nullptr) {
                            active_context[id]->report_mask |= frame.x[1];
                            frame.x[0] = active_context[id]->report_mask;
                        } else {
                            frame.x[0] = 0U;
                        }
                        return true;
                    case abi::v1::guest_hypercall::diagnostic:
#if CONFIG_VERBOSE_DIAGNOSTICS
                        diagnose_guest_mmu(frame);
                        if (frame.x[1] == 1U) {
                            /*
                             * Trap only the bootstrap SCTLR_EL1 write. EL2 applies
                             * the value and returns through its exception path,
                             * making the first translated fetch observable and
                             * context synchronized.
                             */
                            u64 hcr = 0U;
                            __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
                            hcr |= 1ULL << 26U;   // TVM
                            hcr &= ~(1ULL << 7U); // no stale VI on first run
                            __asm__ volatile("msr hcr_el2, %0; isb" ::"r"(hcr) : "memory");
                            guest_mmu_enable_armed[id] = true;
                        }
                        frame.x[0] = 0U;
#else
                        frame.x[0] = static_cast<u64>(static_cast<s64>(error_t::denied));
#endif
                        return true;
                    case abi::v1::guest_hypercall::shutdown: {
                        auto* exit = active_exit[id];
                        const u64 reports =
                            active_context[id] == nullptr ? 0U : active_context[id]->report_mask;
                        complete_guest_exit(frame, syndrome, abi::v1::vm_exit_reason::shutdown);
                        if (exit != nullptr)
                            exit->qualification = reports;
                        return true;
                    }
                }
                auto* exit = active_exit[id];
                complete_guest_exit(frame, syndrome, abi::v1::vm_exit_reason::hypercall);
                if (exit != nullptr)
                    exit->qualification = hypercall_number;
                return true;
            }
            if (exception_class == 0x18U && guest_mmu_enable_armed[id]) {
                /*
                 * HCR_EL2.TVM trapped the guest's bootstrap
                 *     MSR SCTLR_EL1, X0
                 * instruction. Apply the requested value at EL2, disarm TVM,
                 * skip the trapped instruction, and resume through EL2 ERET.
                 */
                const u32 trapped_register = static_cast<u32>((syndrome >> 5U) & 0x1fU);
                const u64 trapped_operand =
                    trapped_register == 31U ? 0U : frame.x[trapped_register];
                const u64 post_enable_pc = frame.x[1];
                u64 current_sctlr = 0U;
                u64 hcr = 0U;
                __asm__ volatile("mrs %0, sctlr_el1; mrs %1, hcr_el2"
                                 : "=r"(current_sctlr), "=r"(hcr));
                /*
                 * Emulate the trapped MSR using the Rt field encoded in ESR_EL2.
                 * The guest has already constructed split RX and RW/XN stage-1
                 * mappings and intentionally enables both M and WXN here.
                 */
                const u64 applied_sctlr = sanitize_guest_sctlr_el1(trapped_operand);
#if CONFIG_VERBOSE_DIAGNOSTICS
                console_puts("\n[HV-MMU] phase=el2-enable");
                console_field("ec", exception_class);
                console_field("esr", syndrome);
                console_field("pc", frame.instruction_pointer);
                console_field("trapped_rt", trapped_register);
                console_field("trapped_operand", trapped_operand);
                console_field("current_sctlr", current_sctlr);
                console_field("applied_sctlr", applied_sctlr);
                console_field("resume_pc", post_enable_pc);
                console_field("resume_spsr", frame.status);
                console_field("hcr_before", hcr);
                console_putc('\n');
#endif
                /*
                 * Apply the guest SCTLR value and discard any stale stage-1
                 * translation or instruction-cache state before the first
                 * translated fetch.  The guest built and published its tables
                 * before trapping this write, but the pre-enable AT probe may
                 * have populated translation state under the old SCTLR regime.
                 */
                __asm__ volatile("msr sctlr_el1, %0; dsb ish; tlbi vmalle1is; "
                                 "dsb ish; ic iallu; dsb ish; isb" ::"r"(applied_sctlr)
                                 : "memory");
                hcr |= 1ULL << 26U;   // keep stage-1 control writes trapped
                hcr &= ~(1ULL << 7U); // VI remains clear until explicit injection
                __asm__ volatile("msr hcr_el2, %0; isb" ::"r"(hcr) : "memory");

                u64 combined_par = 0U;
                u64 sctlr_readback = 0U;
                u64 tcr_readback = 0U;
                u64 ttbr0_readback = 0U;
                u64 hcr_readback = 0U;
                __asm__ volatile("at s12e1r, %5; isb; mrs %0, par_el1; "
                                 "mrs %1, sctlr_el1; mrs %2, tcr_el1; "
                                 "mrs %3, ttbr0_el1; mrs %4, hcr_el2"
                                 : "=r"(combined_par), "=r"(sctlr_readback), "=r"(tcr_readback),
                                   "=r"(ttbr0_readback), "=r"(hcr_readback)
                                 : "r"(post_enable_pc)
                                 : "memory");
#if CONFIG_VERBOSE_DIAGNOSTICS
                console_puts("[HV-MMU] phase=el2-resume");
                console_field("combined_par", combined_par);
                console_field("sctlr", sctlr_readback);
                console_field("tcr", tcr_readback);
                console_field("ttbr0", ttbr0_readback);
                console_field("hcr", hcr_readback);
                console_field("elr", post_enable_pc);
                console_field("spsr", frame.status);
                console_putc('\n');
#endif

                guest_mmu_enable_armed[id] = false;
                /*
                 * Resume directly at the mapped post-enable label.  Returning
                 * to the guest's ISB/ERET sequence would introduce a second
                 * exception-return transition whose ELR_EL1/SPSR_EL1 state is
                 * unrelated to EL2's saved guest frame.  The trapped MSR has
                 * already completed here and EL2 ERET is the required context
                 * synchronization event.
                 */
                frame.instruction_pointer = post_enable_pc;
                return true;
            }
            if (exception_class == 0x18U) {
                if (emulate_native_gic_system_register(frame, syndrome))
                    return true;
                constexpr u32 sctlr_el1 = 0xc080U;
                constexpr u32 ttbr0_el1 = 0xc100U;
                constexpr u32 ttbr1_el1 = 0xc101U;
                constexpr u32 tcr_el1 = 0xc102U;
                constexpr u32 mair_el1 = 0xc510U;
                const u32 encoding = trapped_system_register(syndrome);
                const u32 target = static_cast<u32>((syndrome >> 5U) & 0x1fU);
                const bool read = (syndrome & 1U) != 0U;
                u64 value = target == 31U ? 0U : frame.x[target];
                bool handled = true;
                if (!read) {
                    switch (encoding) {
                        case sctlr_el1:
                            value = sanitize_guest_sctlr_el1(value);
                            __asm__ volatile("msr sctlr_el1, %0" ::"r"(value) : "memory");
                            break;
                        case ttbr0_el1:
                            value &= 0x0000fffffffff000ULL;
                            __asm__ volatile("msr ttbr0_el1, %0" ::"r"(value) : "memory");
                            break;
                        case ttbr1_el1:
                            value &= 0x0000fffffffff000ULL;
                            __asm__ volatile("msr ttbr1_el1, %0" ::"r"(value) : "memory");
                            break;
                        case tcr_el1:
                            value = sanitize_guest_tcr_el1(value);
                            __asm__ volatile("msr tcr_el1, %0" ::"r"(value) : "memory");
                            break;
                        case mair_el1:
                            __asm__ volatile("msr mair_el1, %0" ::"r"(value) : "memory");
                            break;
                        default:
                            handled = false;
                            break;
                    }
                } else {
                    switch (encoding) {
                        case sctlr_el1:
                            __asm__ volatile("mrs %0, sctlr_el1" : "=r"(value));
                            break;
                        case ttbr0_el1:
                            __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(value));
                            break;
                        case ttbr1_el1:
                            __asm__ volatile("mrs %0, ttbr1_el1" : "=r"(value));
                            break;
                        case tcr_el1:
                            __asm__ volatile("mrs %0, tcr_el1" : "=r"(value));
                            break;
                        case mair_el1:
                            __asm__ volatile("mrs %0, mair_el1" : "=r"(value));
                            break;
                        default:
                            handled = false;
                            break;
                    }
                    if (handled && target != 31U)
                        frame.x[target] = value;
                }
                if (handled) {
                    __asm__ volatile("dsb ish; isb" ::: "memory");
                    frame.instruction_pointer += 4U;
                    return true;
                }
                auto* exit = active_exit[id];
                complete_guest_exit(frame, syndrome, abi::v1::vm_exit_reason::system_register);
                if (exit != nullptr)
                    exit->qualification = encoding;
                return true;
            }
            if (exception_class == 0x01U) {
                frame.instruction_pointer += 4U;
                complete_guest_exit(frame, syndrome, abi::v1::vm_exit_reason::wait);
                return true;
            }
            const bool data_abort = exception_class == 0x24U || exception_class == 0x25U;
            const bool valid_access = data_abort && ((syndrome >> 24U) & 1U) != 0U;
            if (valid_access) {
                u64 hpfar = 0U;
                __asm__ volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
                const u64 ipa = guest_fault_ipa(syndrome, exception::fault_address(2U), hpfar);
                if (emulate_native_gic_mmio(frame, syndrome, ipa))
                    return true;
            }
            const auto reason = valid_access ? abi::v1::vm_exit_reason::mmio
                                : data_abort ? abi::v1::vm_exit_reason::stage2_fault
                                             : abi::v1::vm_exit_reason::unexpected;
#if CONFIG_VERBOSE_DIAGNOSTICS
            u64 hpfar = 0U;
            __asm__ volatile("mrs %0, hpfar_el2" : "=r"(hpfar));
            console_puts("\n[HV-TRAP] phase=guest-bootstrap");
            console_field("ec", exception_class);
            console_field("esr", syndrome);
            console_field("far", exception::fault_address(2U));
            console_field("hpfar", hpfar);
            console_field("elr", frame.instruction_pointer);
            console_field("spsr", frame.status);
            console_putc('\n');
#endif
            auto* completed_exit = active_exit[id];
            complete_guest_exit(frame, syndrome, reason);
            if (reason == abi::v1::vm_exit_reason::mmio) {
                if (completed_exit != nullptr)
                    completed_exit->qualification =
                        mmio_qualification(syndrome, completed_exit->qualification);
            }
            return true;
        }

        if (exception_class != hvc64_exception_class)
            return false;
        __atomic_fetch_add(&call_count[id], 1U, __ATOMIC_RELAXED);

        switch (static_cast<call_id>(frame.x[0])) {
            case call_id::version:
                frame.x[0] = abi_version;
                break;
            case call_id::cpu_id:
                frame.x[0] = static_cast<u64>(id);
                break;
            case call_id::test:
                frame.x[0] = frame.x[1];
                break;
            case call_id::host_configure: {
                u64 hcr = 0U;
                u64 vtr = 0U;
                __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
                __asm__ volatile("mrs %0, ich_vtr_el2" : "=r"(vtr));
                hcr |= 1ULL << 31U;
                __asm__ volatile("msr hcr_el2, %0; isb" ::"r"(hcr) : "memory");
                __atomic_store_n(&ich_vtr[id], vtr, __ATOMIC_RELEASE);
                frame.x[0] = 0U;
                break;
            }
            case call_id::stage2_invalidate:
                __asm__ volatile("dsb ishst; tlbi vmalls12e1is; dsb ish; isb" ::: "memory");
                frame.x[0] = 0U;
                break;
            case call_id::vgic_maintenance: {
                u64 misr = 0U;
                __asm__ volatile("mrs %0, ich_misr_el2" : "=r"(misr));
                frame.x[0] = misr;
                break;
            }
            case call_id::guest_run: {
                auto* context = reinterpret_cast<guest_context_layout*>(frame.x[3]);
                auto* exit = reinterpret_cast<exit_layout*>(frame.x[4]);
                if (context == nullptr || exit == nullptr || frame.x[2] == 0U) {
                    frame.x[0] = static_cast<u64>(static_cast<s64>(error_t::invalid_argument));
                    break;
                }
                copy_frame(host_frame[id], frame);
                active_context[id] = context;
                active_exit[id] = exit;
                guest_active[id] = true;
                __atomic_store_n(&guest_irq_acknowledged[id], false, __ATOMIC_RELEASE);
                __asm__ volatile("mrs %0, hcr_el2; mrs %1, vttbr_el2; mrs %2, vtcr_el2; "
                                 "mrs %3, cntvoff_el2"
                                 : "=r"(saved_hcr[id]), "=r"(saved_vttbr[id]), "=r"(saved_vtcr[id]),
                                   "=r"(saved_cntvoff[id]));
                save_el1_system_state(saved_host_el1[id]);
                const u64 run_flags = frame.x[1];
                const u64 vmid = run_flags & 0xffffU;
                const bool inject_virtual_irq = (run_flags & (1ULL << 16U)) != 0U;
#if CONFIG_GUEST_ZEPHYR
                route_guest_uart_irq();
                if (context->native_gic &&
                    __atomic_exchange_n(&guest_uart_irq_pending[id], false, __ATOMIC_ACQ_REL))
                    context->gic_pending |= 1ULL << 33U;
#endif
                if (!inject_virtual_irq)
                    context->report_mask = 0U;
                const u64 vttbr = (vmid << 48U) | (frame.x[2] & 0x0000fffffffff000ULL);
                constexpr u64 vtcr_res1 = 1ULL << 31U;
                constexpr u64 vtcr_ps_40bit = 2ULL << 16U;
                constexpr u64 vtcr_tg0_4k = 0ULL << 14U;
                constexpr u64 vtcr_sh0_inner = 3ULL << 12U;
                constexpr u64 vtcr_orgn0_wbwa = 1ULL << 10U;
                constexpr u64 vtcr_irgn0_wbwa = 1ULL << 8U;
                constexpr u64 vtcr_sl0_level1 = 1ULL << 6U;
                constexpr u64 vtcr_t0sz_32bit = 32ULL;
                constexpr u64 vtcr = vtcr_res1 | vtcr_ps_40bit | vtcr_tg0_4k | vtcr_sh0_inner |
                                     vtcr_orgn0_wbwa | vtcr_irgn0_wbwa | vtcr_sl0_level1 |
                                     vtcr_t0sz_32bit;
                u64 hcr = saved_hcr[id] | 1ULL | (1ULL << 31U) | (1ULL << 3U) | (1ULL << 4U) |
                          (1ULL << 5U) | (1ULL << 13U) | (1ULL << 14U);
                /*
                 * VI is per-vCPU run state, not persistent host HCR state.
                 * Clear a stale saved value before applying this run's request.
                 */
                hcr &= ~(1ULL << 7U);
                hcr |= 1ULL << 26U;
                if (inject_virtual_irq ||
#if CONFIG_GUEST_ZEPHYR
                    (context->native_gic && native_gic_deliverable(*context) != 0U))
#else
                    false)
#endif
                    hcr |= 1ULL << 7U;
                __asm__ volatile("dsb ishst; tlbi vmalls12e1is; dsb ish; "
                                 "msr vtcr_el2, %0; msr vttbr_el2, %1; msr hcr_el2, %2; "
                                 "msr cntvoff_el2, %3; isb" ::"r"(vtcr),
                                 "r"(vttbr), "r"(hcr), "r"(requested_counter_offset[id])
                                 : "memory");
                u64 programmed_vtcr = 0U;
                u64 programmed_vttbr = 0U;
                u64 programmed_hcr = 0U;
                u64 programmed_cntvoff = 0U;
                __asm__ volatile("mrs %0, vtcr_el2; mrs %1, vttbr_el2; mrs %2, hcr_el2; "
                                 "mrs %3, cntvoff_el2"
                                 : "=r"(programmed_vtcr), "=r"(programmed_vttbr),
                                   "=r"(programmed_hcr), "=r"(programmed_cntvoff));
                if (programmed_vtcr != vtcr || programmed_vttbr != vttbr ||
                    (programmed_hcr & 1ULL) == 0U ||
                    programmed_cntvoff != requested_counter_offset[id]) {
                    if (exit != nullptr) {
                        exit->reason = abi::v1::vm_exit_reason::unexpected;
                        exit->syndrome = programmed_vtcr;
                        exit->fault_address = programmed_vttbr;
                        exit->guest_pc = context->pc;
                        exit->qualification = programmed_cntvoff;
                    }
                    load_el1_system_state(saved_host_el1[id]);
                    __asm__ volatile("msr hcr_el2, %0; msr vttbr_el2, %1; msr vtcr_el2, %2; "
                                     "msr cntvoff_el2, %3; isb" ::"r"(saved_hcr[id]),
                                     "r"(saved_vttbr[id]), "r"(saved_vtcr[id]),
                                     "r"(saved_cntvoff[id])
                                     : "memory");
                    guest_active[id] = false;
                    active_context[id] = nullptr;
                    active_exit[id] = nullptr;
                    copy_frame(frame, host_frame[id]);
                    frame.x[0] = static_cast<u64>(static_cast<s64>(error_t::invalid_argument));
                    break;
                }
                load_el1_system_state(*context);
                frame_from_context(frame, *context);
                break;
            }
            default:
                frame.x[0] = static_cast<u64>(static_cast<s64>(error_t::invalid_argument));
                break;
        }
        return true;
    }
} // namespace sys::arch::hypervisor
