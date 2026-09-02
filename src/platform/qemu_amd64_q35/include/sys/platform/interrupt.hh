#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::platform::interrupt
{
    inline constexpr uintptr_t lapic_base = 0xfee00000ULL;
    inline constexpr uintptr_t ioapic_base = 0xfec00000ULL;

    inline constexpr irq_id_t reschedule_ipi = 32U;
    inline constexpr irq_id_t tlb_shootdown_ipi = 33U;
    inline constexpr irq_id_t virtual_timer_irq = 32U;
    inline constexpr irq_id_t com1_irq = 4U;

    inline constexpr u64 register_wait_limit = 1000000ULL;

    [[nodiscard]] inline constexpr bool userspace_assignable(irq_id_t irq) noexcept {
        return irq >= 32U && irq <= 239U;
    }

    [[nodiscard]] inline volatile u32& lapic_reg32(uintptr_t offset) noexcept {
        return *reinterpret_cast<volatile u32*>(lapic_base + offset);
    }

    [[nodiscard]] inline volatile u32& ioapic_reg32(uintptr_t offset) noexcept {
        return *reinterpret_cast<volatile u32*>(ioapic_base + offset);
    }

    [[nodiscard]] inline u32 lapic_read(uintptr_t offset) noexcept {
        return lapic_reg32(offset);
    }

    inline void lapic_write(uintptr_t offset, u32 value) noexcept {
        lapic_reg32(offset) = value;
    }

    [[nodiscard]] inline u32 ioapic_read(u32 index) noexcept {
        ioapic_reg32(0U) = index;
        return ioapic_reg32(4U);
    }

    inline void ioapic_write(u32 index, u32 value) noexcept {
        ioapic_reg32(0U) = index;
        ioapic_reg32(4U) = value;
    }

    inline void initialize_global() noexcept {
        /* Mask legacy PIC (8259) - disable by masking all IRQs */
        __asm__ volatile("outb %0, $0x21" : : "a"(0xffU));
        __asm__ volatile("outb %0, $0xa1" : : "a"(0xffU));

        /* Enable LAPIC via MSR */
        u64 apic_base;
        __asm__ volatile("rdmsr" : "=a"(apic_base) : "c"(0x1bU) : "edx");
        apic_base |= (1ULL << 11U);
        __asm__ volatile("wrmsr" : : "c"(0x1bU), "a"(apic_base & 0xffffffffULL),
                         "d"((apic_base >> 32U) & 0xffffffffULL));

        /* Set LAPIC spurious interrupt vector (vector 255, APIC enable bit 8) */
        lapic_write(0xf0U, lapic_read(0xf0U) | 0x100U | 0xffU);
    }

    [[nodiscard]] inline error_t initialize_cpu() noexcept {
        /* Each CPU initializes its LAPIC */
        lapic_write(0xf0U, lapic_read(0xf0U) | 0x100U | 0xffU);
        return error_t::success;
    }

    [[nodiscard]] inline error_t initialize() noexcept {
        return error_t::success;
    }

    [[nodiscard]] inline irq_id_t acknowledge() noexcept {
        /* Read ISR/IRR to determine which vector fired */
        const u32 vector = lapic_read(0x60U) & 0xffU;
        return vector >= 32U ? static_cast<irq_id_t>(vector) : 0U;
    }

    inline void complete(irq_id_t) noexcept {
        /* Send EOI to LAPIC */
        lapic_write(0xb0U, 0U);
    }

    inline void deactivate(irq_id_t) noexcept {
        /* No-op for LAPIC */
    }

    inline void mask(irq_id_t irq) noexcept {
        if (irq < 32U)
            return;
        const u32 entry_index = (irq - 32U) * 2U;
        u64 entry = static_cast<u64>(ioapic_read(entry_index)) |
                   (static_cast<u64>(ioapic_read(entry_index + 1U)) << 32U);
        entry |= (1ULL << 16U);
        ioapic_write(entry_index, static_cast<u32>(entry));
        ioapic_write(entry_index + 1U, static_cast<u32>(entry >> 32U));
    }

    inline void unmask(irq_id_t irq) noexcept {
        if (irq < 32U)
            return;
        const u32 entry_index = (irq - 32U) * 2U;
        u64 entry = static_cast<u64>(ioapic_read(entry_index)) |
                   (static_cast<u64>(ioapic_read(entry_index + 1U)) << 32U);
        entry &= ~(1ULL << 16U);
        ioapic_write(entry_index, static_cast<u32>(entry));
        ioapic_write(entry_index + 1U, static_cast<u32>(entry >> 32U));
    }

    inline void route_to_current_cpu(irq_id_t irq) noexcept {
        if (irq < 32U)
            return;
        const u32 entry_index = (irq - 32U) * 2U;
        const u32 cpu_id = sys::arch::cpu::read_apic_id();
        u64 entry = static_cast<u64>(ioapic_read(entry_index)) |
                   (static_cast<u64>(ioapic_read(entry_index + 1U)) << 32U);
        entry = (entry & 0x00ffffffULL) | (static_cast<u64>(cpu_id) << 32U);
        ioapic_write(entry_index, static_cast<u32>(entry));
        ioapic_write(entry_index + 1U, static_cast<u32>(entry >> 32U));
    }

    [[nodiscard]] inline error_t configure(irq_id_t irq, bool edge_triggered) noexcept {
        if (irq < 32U)
            return error_t::invalid_argument;
        const u32 entry_index = (irq - 32U) * 2U;
        u64 entry = static_cast<u64>(ioapic_read(entry_index)) |
                   (static_cast<u64>(ioapic_read(entry_index + 1U)) << 32U);
        entry = (entry & ~(0xfULL)) | (irq & 0xffULL);
        if (edge_triggered)
            entry &= ~(1ULL << 15U);
        else
            entry |= (1ULL << 15U);
        entry |= (1ULL << 11U);
        ioapic_write(entry_index, static_cast<u32>(entry));
        ioapic_write(entry_index + 1U, static_cast<u32>(entry >> 32U));
        return error_t::success;
    }

    inline void send_ipi_all_others(irq_id_t irq) noexcept {
        if (irq >= 32U && irq <= 255U) {
            const u32 icr_high = lapic_read(0x310U);
            lapic_write(0x310U, icr_high & 0x00ffffffU);
            lapic_write(0x300U, (1U << 19U) | (irq & 0xffU));
        }
    }

    inline void send_ipi(cpu_id_t cpu, irq_id_t irq) noexcept {
        if (irq >= 32U && irq <= 255U) {
            const u32 cpu_apic_id = static_cast<u32>(cpu);
            lapic_write(0x310U, cpu_apic_id << 24U);
            lapic_write(0x300U, irq & 0xffU);
        }
    }
} // namespace sys::platform::interrupt
