#pragma once

#include <abi/sys/v1/hypervisor.hh>
#include <sys/arch/cpu.hh>
#include <sys/arch/exception.hh>
#include <sys/types.hh>

namespace sys::arch::hypervisor
{
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
    inline guest_context_layout saved_host_el1[maximum_cpu_count]{};


    inline void save_el1_system_state(guest_context_layout& state) noexcept
    {
        __asm__ volatile(
            "mrs %0, sp_el0; mrs %1, sp_el1; mrs %2, elr_el1; mrs %3, spsr_el1; "
            "mrs %4, sctlr_el1; mrs %5, tcr_el1; mrs %6, ttbr0_el1; "
            "mrs %7, ttbr1_el1; mrs %8, mair_el1; mrs %9, vbar_el1; "
            "mrs %10, tpidr_el0; mrs %11, tpidr_el1; "
            "mrs %12, cntv_ctl_el0; mrs %13, cntv_cval_el0"
            : "=r"(state.sp_el0), "=r"(state.sp_el1), "=r"(state.elr_el1),
              "=r"(state.spsr_el1), "=r"(state.sctlr_el1), "=r"(state.tcr_el1),
              "=r"(state.ttbr0_el1), "=r"(state.ttbr1_el1), "=r"(state.mair_el1),
              "=r"(state.vbar_el1), "=r"(state.tpidr_el0), "=r"(state.tpidr_el1),
              "=r"(state.cntv_ctl_el0), "=r"(state.cntv_cval_el0));
    }

    inline void load_el1_system_state(const guest_context_layout& state) noexcept
    {
        __asm__ volatile(
            "msr sp_el0, %0; msr sp_el1, %1; msr elr_el1, %2; msr spsr_el1, %3; "
            "msr tcr_el1, %5; msr ttbr0_el1, %6; msr ttbr1_el1, %7; "
            "msr mair_el1, %8; msr vbar_el1, %9; "
            "msr tpidr_el0, %10; msr tpidr_el1, %11; "
            "msr cntv_ctl_el0, %12; msr cntv_cval_el0, %13; "
            "msr sctlr_el1, %4; isb"
            :: "r"(state.sp_el0), "r"(state.sp_el1), "r"(state.elr_el1),
               "r"(state.spsr_el1), "r"(state.sctlr_el1), "r"(state.tcr_el1),
               "r"(state.ttbr0_el1), "r"(state.ttbr1_el1), "r"(state.mair_el1),
               "r"(state.vbar_el1), "r"(state.tpidr_el0), "r"(state.tpidr_el1),
               "r"(state.cntv_ctl_el0), "r"(state.cntv_cval_el0)
            : "memory");
    }

    inline void copy_frame(exception::frame_t& destination,
                           const exception::frame_t& source) noexcept
    {
        for (u32 index = 0U; index < 31U; ++index) destination.x[index] = source.x[index];
        destination.vector = source.vector;
        destination.stack_pointer = source.stack_pointer;
        destination.instruction_pointer = source.instruction_pointer;
        destination.status = source.status;
    }

    inline void frame_from_context(exception::frame_t& frame,
                                   const guest_context_layout& context) noexcept
    {
        for (u32 index = 0U; index < 31U; ++index) frame.x[index] = context.x[index];
        frame.stack_pointer = context.sp_el0;
        frame.instruction_pointer = context.pc;
        frame.status = context.pstate;
    }

    inline void context_from_frame(guest_context_layout& context,
                                   const exception::frame_t& frame) noexcept
    {
        for (u32 index = 0U; index < 31U; ++index) context.x[index] = frame.x[index];
        context.sp_el0 = frame.stack_pointer;
        context.pc = frame.instruction_pointer;
        context.pstate = frame.status;
    }

    [[nodiscard]] inline u64 call(call_id function, u64 argument0 = 0U,
                                  u64 argument1 = 0U, u64 argument2 = 0U,
                                  u64 argument3 = 0U) noexcept
    {
        register u64 x0 __asm__("x0") = static_cast<u64>(function);
        register u64 x1 __asm__("x1") = argument0;
        register u64 x2 __asm__("x2") = argument1;
        register u64 x3 __asm__("x3") = argument2;
        register u64 x4 __asm__("x4") = argument3;
        __asm__ volatile("hvc #0" : "+r"(x0)
                         : "r"(x1), "r"(x2), "r"(x3), "r"(x4)
                         : "x5", "x6", "x7", "memory");
        return x0;
    }

    [[nodiscard]] inline error_t configure_host() noexcept
    {
        return call(call_id::host_configure) == 0U ? error_t::success
                                                    : error_t::unsupported;
    }

    inline void invalidate_stage2(u16 vmid) noexcept
    {
        (void)call(call_id::stage2_invalidate, vmid);
    }

    template <typename Context, typename Exit>
    [[nodiscard]] inline error_t run_guest(u16 vmid, paddr_t stage2_root,
                                           Context& context, Exit& exit) noexcept
    {
        static_assert(sizeof(Context) == sizeof(guest_context_layout));
        static_assert(sizeof(Exit) == sizeof(exit_layout));
        const u64 result = call(call_id::guest_run, vmid, stage2_root,
                                reinterpret_cast<u64>(&context),
                                reinterpret_cast<u64>(&exit));
        return static_cast<error_t>(static_cast<s64>(result));
    }

    [[nodiscard]] inline error_t initialize() noexcept { return error_t::success; }

    [[nodiscard]] inline bool initialize_cpu() noexcept
    {
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count) return false;
        if (call(call_id::version) != abi_version) return false;
        if (call(call_id::cpu_id) != static_cast<u64>(id)) return false;
        if (call(call_id::test, test_cookie) != test_cookie) return false;
        __atomic_fetch_or(&verified_cpu_mask, 1U << id, __ATOMIC_RELEASE);
        return true;
    }

    [[nodiscard]] inline u32 verified_count() noexcept
    {
        return static_cast<u32>(__builtin_popcount(
            __atomic_load_n(&verified_cpu_mask, __ATOMIC_ACQUIRE)));
    }

    inline void console_putc(char value) noexcept
    {
        auto* uart = reinterpret_cast<volatile u32*>(0x09000000ULL);
        while ((uart[6] & (1U << 5U)) != 0U) {}
        uart[0] = static_cast<u32>(static_cast<unsigned char>(value));
    }

    inline void complete_guest_exit(exception::frame_t& frame, u64 syndrome,
                                    abi::v1::vm_exit_reason reason) noexcept
    {
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
            exit->qualification = hpfar;
        }
        load_el1_system_state(saved_host_el1[id]);
        __asm__ volatile("msr hcr_el2, %0; msr vttbr_el2, %1; msr vtcr_el2, %2; isb"
                         :: "r"(saved_hcr[id]), "r"(saved_vttbr[id]),
                            "r"(saved_vtcr[id]) : "memory");
        guest_active[id] = false;
        active_context[id] = nullptr;
        active_exit[id] = nullptr;
        copy_frame(frame, host_frame[id]);
        frame.x[0] = static_cast<u64>(static_cast<s64>(error_t::success));
    }

    [[nodiscard]] inline bool dispatch(exception::frame_t& frame,
                                       u64 syndrome) noexcept
    {
        constexpr u64 exception_class_mask = 0x3fULL;
        constexpr u64 hvc64_exception_class = 0x16ULL;
        const u64 exception_class = (syndrome >> 26U) & exception_class_mask;
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count) return false;

        if (guest_active[id]) {
            if (exception_class == hvc64_exception_class) {
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
                case abi::v1::guest_hypercall::irq_acknowledge:
                    frame.x[0] = 0U;
                    return true;
                case abi::v1::guest_hypercall::shutdown:
                    complete_guest_exit(frame, syndrome,
                                        abi::v1::vm_exit_reason::shutdown);
                    return true;
                }
                complete_guest_exit(frame, syndrome,
                                    abi::v1::vm_exit_reason::hypercall);
                return true;
            }
            const auto reason = (exception_class == 0x24U || exception_class == 0x25U)
                ? abi::v1::vm_exit_reason::stage2_fault
                : abi::v1::vm_exit_reason::unexpected;
            complete_guest_exit(frame, syndrome, reason);
            return true;
        }

        if (exception_class != hvc64_exception_class) return false;
        __atomic_fetch_add(&call_count[id], 1U, __ATOMIC_RELAXED);

        switch (static_cast<call_id>(frame.x[0])) {
        case call_id::version: frame.x[0] = abi_version; break;
        case call_id::cpu_id: frame.x[0] = static_cast<u64>(id); break;
        case call_id::test: frame.x[0] = frame.x[1]; break;
        case call_id::host_configure: {
            u64 hcr = 0U;
            __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
            hcr |= (1ULL << 31U) | (1ULL << 13U) | (1ULL << 14U);
            __asm__ volatile("msr hcr_el2, %0; isb" :: "r"(hcr) : "memory");
            frame.x[0] = 0U;
            break;
        }
        case call_id::stage2_invalidate:
            __asm__ volatile("dsb ishst; tlbi vmalls12e1is; dsb ish; isb" ::: "memory");
            frame.x[0] = 0U;
            break;
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
            __asm__ volatile("mrs %0, hcr_el2; mrs %1, vttbr_el2; mrs %2, vtcr_el2"
                             : "=r"(saved_hcr[id]), "=r"(saved_vttbr[id]),
                               "=r"(saved_vtcr[id]));
            save_el1_system_state(saved_host_el1[id]);
            const u64 vmid = frame.x[1] & 0xffffU;
            const u64 vttbr = (vmid << 48U) | (frame.x[2] & 0x0000fffffffff000ULL);
            constexpr u64 vtcr_res1 = 1ULL << 31U;
            constexpr u64 vtcr_ps_40bit = 2ULL << 16U;
            constexpr u64 vtcr_tg0_4k = 0ULL << 14U;
            constexpr u64 vtcr_sh0_inner = 3ULL << 12U;
            constexpr u64 vtcr_orgn0_wbwa = 1ULL << 10U;
            constexpr u64 vtcr_irgn0_wbwa = 1ULL << 8U;
            constexpr u64 vtcr_sl0_level1 = 1ULL << 6U;
            constexpr u64 vtcr_t0sz_32bit = 32ULL;
            constexpr u64 vtcr = vtcr_res1 | vtcr_ps_40bit | vtcr_tg0_4k
                | vtcr_sh0_inner | vtcr_orgn0_wbwa | vtcr_irgn0_wbwa
                | vtcr_sl0_level1 | vtcr_t0sz_32bit;
            u64 hcr = saved_hcr[id] | 1ULL | (1ULL << 31U)
                | (1ULL << 3U) | (1ULL << 4U) | (1ULL << 5U);
            __asm__ volatile(
                "dsb ishst; tlbi vmalls12e1is; dsb ish; "
                "msr vtcr_el2, %0; msr vttbr_el2, %1; msr hcr_el2, %2; "
                "isb"
                :: "r"(vtcr), "r"(vttbr), "r"(hcr) : "memory");
            u64 programmed_vtcr = 0U;
            u64 programmed_vttbr = 0U;
            u64 programmed_hcr = 0U;
            __asm__ volatile("mrs %0, vtcr_el2; mrs %1, vttbr_el2; mrs %2, hcr_el2"
                             : "=r"(programmed_vtcr), "=r"(programmed_vttbr),
                               "=r"(programmed_hcr));
            if (programmed_vtcr != vtcr || programmed_vttbr != vttbr
                || (programmed_hcr & 1ULL) == 0U) {
                if (exit != nullptr) {
                    exit->reason = abi::v1::vm_exit_reason::unexpected;
                    exit->syndrome = programmed_vtcr;
                    exit->fault_address = programmed_vttbr;
                    exit->guest_pc = context->pc;
                    exit->qualification = programmed_hcr;
                }
                load_el1_system_state(saved_host_el1[id]);
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
