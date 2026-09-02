#pragma once

#include <sys/types.hh>

#include <abi/sys/v1/control_plane.hh>

/*
 * vPL011: a virtual ARM PL011 UART presented to a guest domain.
 *
 * The guest's own PL011 driver touches only a handful of registers, so only
 * those are modeled -- DR, FR (RXFE/TXFF), IMSC (RXIM) and CR are real;
 * everything else in the 4 KiB IPA window is a safe write-and-discard /
 * read-returns-zero. This is the device *model* only: register state and the
 * layout needed to decode a trapped access. Forwarding characters to the
 * console-server, injecting the guest IRQ, and advancing the guest PC all
 * need capabilities, so they stay with the VMM server that holds them
 * (src/user/servers/domain).
 *
 * This is a guest-facing device, not the host's own UART: the host PL011 is
 * owned exclusively by src/user/drivers/serial. A guest never touches real
 * hardware -- its accesses trap to stage-2 and land here.
 */
namespace sys::vmm::vpl011
{
    inline constexpr word_t base_ipa = 0x09000000U;
    inline constexpr word_t size = 0x1000U;
    inline constexpr u16 irq = 33U;

    inline constexpr word_t offset_dr = 0x00U;
    inline constexpr word_t offset_fr = 0x18U;
    inline constexpr word_t offset_cr = 0x30U;
    inline constexpr word_t offset_imsc = 0x38U;

    inline constexpr word_t fr_rxfe = 1U << 4U;
    // TXFF (bit 5) is never set in emulated FR reads: TX is forwarded to the
    // console-server, so the guest never needs to wait for space.
    inline constexpr word_t imsc_rxim = 1U << 4U;

    /*
     * Bit layout of a stage-2 abort's qualification, matching
     * src/arch/arm64/include/sys/arch/hypervisor.hh's mmio_qualification().
     * This is the one genuinely arm64-specific part of the model: a VMX host
     * decodes an exit qualification with a different layout entirely.
     */
    inline constexpr word_t qualification_write_bit = 63U;
    inline constexpr word_t qualification_srt_shift = 55U;
    inline constexpr word_t qualification_srt_mask = 0x1fU;
    inline constexpr word_t pc_field = 31U;
    inline constexpr word_t xzr_register = 31U;

    inline word_t imsc{};
    inline word_t cr{};
    inline bool rx_pending{};
    inline u8 rx_byte{};

    /*
     * TX FIFO. DR writes land here rather than forwarding synchronously:
     * one blocking IPC round trip per guest register write dominated
     * observed serial latency. The model still traps per access -- real
     * hardware register granularity -- but the IPC cost after each trap is
     * not paid per byte. Holds console_write_max_bytes - 1 data bytes plus
     * room for the NUL terminator console::write() expects.
     */
    inline constexpr word_t tx_buffer_capacity = abi::v1::console_write_max_bytes - 1U;
    inline u8 tx_buffer[abi::v1::console_write_max_bytes]{};
    inline word_t tx_length{};

    [[nodiscard]] inline constexpr bool owns(word_t ipa) noexcept {
        return ipa >= base_ipa && ipa < base_ipa + size;
    }

    [[nodiscard]] inline constexpr bool is_write(word_t qualification) noexcept {
        return ((qualification >> qualification_write_bit) & 1U) != 0U;
    }

    [[nodiscard]] inline constexpr word_t source_register(word_t qualification) noexcept {
        return (qualification >> qualification_srt_shift) & qualification_srt_mask;
    }
} // namespace sys::vmm::vpl011
