#pragma once

#include <sys/types.hh>

/*
 * ELF64 image format, as a guest domain's kernel image presents it.
 *
 * Distinct from each arch's own sys/arch/space/elf64.hh, which is the
 * KERNEL's loader for zilch's own userspace binaries: that one runs in
 * kernel context and produces page permissions for an address space. This
 * one is read by an unprivileged VMM in userspace to place a guest kernel
 * into guest-physical memory, so it shares only the on-disk layout, not the
 * loading policy. Keeping them separate is deliberate -- a guest image is
 * untrusted input to a userspace process, and should not reach for the
 * kernel loader.
 */
namespace sys::vmm::elf
{
    // EM_AARCH64. A guest image for another architecture is rejected rather
    // than misparsed; an amd64 host would additionally need EM_X86_64 (62).
    inline constexpr u16 machine_aarch64 = 0xb7U;

    struct header final {
        u8 ident[16U];
        u16 type{};
        u16 machine{};
        u32 version{};
        u64 entry{};
        u64 phoff{};
        u64 shoff{};
        u32 flags{};
        u16 ehsize{};
        u16 phentsize{};
        u16 phnum{};
        u16 shentsize{};
        u16 shnum{};
        u16 shstrndx{};
    } __attribute__((packed));

    struct program_header final {
        u32 type{};
        u32 flags{};
        u64 offset{};
        u64 vaddr{};
        u64 paddr{};
        u64 filesz{};
        u64 memsz{};
        u64 align{};
    } __attribute__((packed));

    struct section_header final {
        u32 name{};
        u32 type{};
        u64 flags{};
        u64 address{};
        u64 offset{};
        u64 size{};
        u32 link{};
        u32 info{};
        u64 addralign{};
        u64 entsize{};
    } __attribute__((packed));

    /*
     * A guest image that is not a valid ELF is not an error: raw binary
     * images are loaded flat instead (see the domain manager's
     * load_guest_image()). This only answers "can the header be trusted".
     */
    [[nodiscard]] inline bool valid(const u8* begin, word_t size) noexcept {
        if (begin == nullptr || size < sizeof(header))
            return false;
        const auto* value = reinterpret_cast<const header*>(begin);
        return value->ident[0] == 0x7fU && value->ident[1] == 'E' && value->ident[2] == 'L' &&
               value->ident[3] == 'F' && value->ident[4] == 2U && value->ident[5] == 1U &&
               value->machine == machine_aarch64 && value->version == 1U;
    }
} // namespace sys::vmm::elf
