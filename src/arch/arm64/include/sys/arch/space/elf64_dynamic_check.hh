#pragma once

/*
 * Deliberately a separate file from address_space.hh: this needs
 * sys::kernel::memory for the physical-page allocator, and
 * sys/kernel/space/address_space.hh already includes
 * sys/arch/space/address_space.hh and embeds it by value, so having
 * address_space.hh itself depend on sys/kernel/memory/manager.hh would
 * create a circular #include (arch/space/address_space.hh ->
 * kernel/memory/manager.hh -> kernel/space/address_space.hh ->
 * arch/space/address_space.hh) that breaks on whichever file is included
 * first. This file sits outside that cycle: nothing includes it back.
 */
#include <sys/arch/space/address_space.hh>
#include <sys/kernel/memory/manager.hh>

namespace sys::arch::space
{
    /*
     * Proves elf64::load_dynamic() (frame-per-page backing) produces
     * identical results to the existing, already-proven elf64::load()
     * (fixed scratch-buffer backing) for every real image this kernel
     * actually boots -- same status/entry/page-count, identical per-page
     * permissions, and byte-identical page contents. This is the
     * de-risking gate before load_dynamic() is ever wired into
     * address_space::initialize() itself; that cutover is a separate,
     * later step and is not performed here.
     */
    [[nodiscard]] inline bool check_dynamic_loader_role(word_t role) noexcept {
        const image_view image = image_for_role(role);
        const auto image_size = static_cast<usize_t>(image.end - image.start);
        const auto* bytes = reinterpret_cast<const u8*>(image.start);

        static u8 storage[elf64::bootstrap_size];
        static elf64::page_permissions static_permissions[elf64::bootstrap_pages];
        static paddr_t backing[elf64::bootstrap_pages];
        static elf64::page_permissions dynamic_permissions[elf64::bootstrap_pages];

        const auto static_result =
            elf64::load(bytes, image_size, user_code, storage, static_permissions);
        const auto dynamic_result = elf64::load_dynamic(
            bytes, image_size, user_code, backing, dynamic_permissions,
            &kernel::memory::allocate_physical_page, &kernel::memory::release_physical_page);

        bool ok = static_result.status == dynamic_result.status &&
                  static_result.entry == dynamic_result.entry &&
                  static_result.pages == dynamic_result.pages;

        for (usize_t page = 0U; ok && page < elf64::bootstrap_pages; ++page) {
            const auto& expected = static_permissions[page];
            const auto& actual = dynamic_permissions[page];
            if (expected.present != actual.present || expected.writable != actual.writable ||
                expected.executable != actual.executable) {
                ok = false;
                break;
            }
            if (!expected.present)
                continue;
            const auto* dynamic_page =
                reinterpret_cast<const u8*>(static_cast<uintptr_t>(backing[page]));
            for (usize_t byte = 0U; byte < memory::page_size; ++byte) {
                if (storage[page * memory::page_size + byte] != dynamic_page[byte]) {
                    ok = false;
                    break;
                }
            }
        }

        for (usize_t page = 0U; page < elf64::bootstrap_pages; ++page) {
            if (backing[page] != 0U)
                (void)kernel::memory::release_physical_page(backing[page]);
        }
        return ok;
    }

    [[nodiscard]] inline bool validate_elf64_dynamic_loader() noexcept {
        const word_t roles[] = {
            memory_server_image_role,
            control_plane_image_role_base,
            domain_manager_image_role,
#if CONFIG_TESTS
            pager_client_image_role_base,
            memory_client_image_role_base,
#endif
            0xffffffffU, // unmatched role -> exercises the default/init fallback image
        };
        for (word_t role : roles) {
            if (!check_dynamic_loader_role(role))
                return false;
        }
        return true;
    }
} // namespace sys::arch::space
