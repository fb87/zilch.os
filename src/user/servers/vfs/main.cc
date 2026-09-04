#include <string.h>

#include <sys/control.hh>
#include <sys/ext2/reader.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/control_plane.hh>
#include <abi/sys/v1/memory.hh>
#include <abi/sys/v1/virtio.hh>
#include <abi/sys/v1/vfs.hh>

#include "ramfs.hh"

/*
 * The VFS server: mounts the ext2 disk read-only, serves a flat writable
 * /tmp, and answers open/read/write/close/stat/readdir/seek over IPC.
 *
 * Not a control_plane_role -- wired exactly like the block and serial
 * drivers, a direct process_create in root_graph.hh's supervise() outside
 * the fixed five-slot loop, with its own service endpoint and readiness
 * badge.
 *
 * Handles are a single global table, not namespaced per client. A client
 * could technically operate on another client's handle. This userspace is
 * a cooperating set of servers and spawned programs, not yet a hardened
 * multi-tenant one -- the same trust boundary every other server here
 * currently assumes -- and closing that gap is future work, not an
 * oversight in this pass.
 */
namespace
{
    namespace abi = sys::abi::v1;
    namespace native = sys::native;

    inline constexpr sys::capability_id_t service_endpoint = native::service_endpoint;

    /*
     * Received from root. block_service_endpoint and block_shared_frame are
     * the SAME two capabilities root already holds for its own
     * verify_block_service() -- see root_graph.hh's vfs wiring -- so VFS
     * becomes a third client of the block driver's one shared payload
     * frame, safe for the same reason root and the driver already share it:
     * every access is one synchronous call at a time.
     */
    inline constexpr sys::capability_id_t block_service_endpoint_selector = 20U;
    inline constexpr sys::capability_id_t block_shared_frame_selector = 21U;
    inline constexpr sys::capability_id_t client_shared_frame_selector = 22U;

    inline constexpr sys::word_t block_scratch_address = 0x20060000U;
    inline constexpr sys::word_t client_scratch_address = 0x20061000U;

    [[nodiscard]] inline volatile sys::u8* block_buffer() noexcept {
        return reinterpret_cast<volatile sys::u8*>(static_cast<sys::uintptr_t>(block_scratch_address));
    }

    [[nodiscard]] inline sys::u8* client_buffer() noexcept {
        return reinterpret_cast<sys::u8*>(static_cast<sys::uintptr_t>(client_scratch_address));
    }

    /*
     * ext2::reader's one I/O primitive: exactly 1024 bytes at a given
     * 1024-byte-unit index. Each unit is two 512-byte sectors, so this
     * issues two block_operation::read calls and copies each result out of
     * the block driver's shared frame before issuing the next -- reading
     * ahead into the same buffer would overwrite the first sector's bytes
     * before they were captured.
     */
    [[nodiscard]] bool read_kib(void*, sys::u32 index, sys::u8 (&destination)[1024U]) noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        for (sys::u32 half = 0U; half < 2U; ++half) {
            const sys::word_t sector = static_cast<sys::word_t>(index) * 2U + half;
            const auto reply = sys::ipc_call(block_service_endpoint_selector,
                                             static_cast<sys::word_t>(abi::block_operation::read),
                                             sector);
            if (reply.status != success || reply.message0 != success)
                return false;
            for (sys::u32 byte = 0U; byte < abi::block_sector_size; ++byte)
                destination[half * abi::block_sector_size + byte] = block_buffer()[byte];
        }
        return true;
    }

    sys::ext2::filesystem fs{};
    /*
     * Absent is not a failure, the same principle block_check::absent
     * establishes for the block driver: a machine booted with no disk
     * attached (tools/run/run.sh's BLOCK_IMAGE=-) correctly finds nothing
     * to mount, and refusing to become ready over that would make an
     * optional device mandatory. ext2 paths report not_found while this is
     * false; /tmp works regardless, since ramfs needs no disk at all.
     */
    bool ext2_mounted = false;

    enum class handle_kind : sys::u8 { closed, ext2_file, ext2_dir, ramfs_file };

    struct open_file final {
        handle_kind kind{handle_kind::closed};
        sys::ext2::inode node{}; // ext2_file / ext2_dir
        int ramfs_index{-1};     // ramfs_file
        sys::u64 position{};
    };

    inline constexpr int max_open = static_cast<int>(abi::vfs_max_open_files);
    open_file table[max_open]{};

    [[nodiscard]] int allocate_handle() noexcept {
        for (int index = 0; index < max_open; ++index) {
            if (table[index].kind == handle_kind::closed)
                return index;
        }
        return -1;
    }

    [[nodiscard]] bool valid_handle(sys::word_t handle) noexcept {
        return handle < static_cast<sys::word_t>(max_open) &&
               table[handle].kind != handle_kind::closed;
    }

    [[nodiscard]] bool under_tmp(const char* path) noexcept {
        return strncmp(path, "/tmp", 4U) == 0 && (path[4] == '\0' || path[4] == '/');
    }

    /* "/tmp" and "/tmp/" both name the directory itself, with no ramfs
     * entry backing them; anything past that is a flat filename. */
    [[nodiscard]] const char* tmp_name(const char* path) noexcept {
        const char* cursor = path + 4U;
        if (*cursor == '/')
            ++cursor;
        return cursor;
    }

    struct handler_result final {
        sys::word_t status;
        sys::word_t value1;
        sys::word_t value2;
    };

    handler_result do_open(sys::word_t path_length, sys::word_t flags) noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        if (path_length == 0U || path_length >= abi::vfs_max_path)
            return {static_cast<sys::word_t>(sys::error_t::invalid_argument), 0U, 0U};
        char path[abi::vfs_max_path + 1U];
        (void)memcpy(path, client_buffer(), path_length);
        path[path_length] = '\0';

        const int slot = allocate_handle();
        if (slot < 0)
            return {static_cast<sys::word_t>(sys::error_t::no_memory), 0U, 0U};

        if (under_tmp(path)) {
            const char* name = tmp_name(path);
            if (*name == '\0')
                return {static_cast<sys::word_t>(sys::error_t::invalid_argument), 0U, 0U};
            int ramfs_index = sys::vfs::ramfs::find(name);
            if (ramfs_index < 0) {
                if ((flags & abi::vfs_open_creat) == 0U)
                    return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
                ramfs_index = sys::vfs::ramfs::create(name);
                if (ramfs_index < 0)
                    return {static_cast<sys::word_t>(sys::error_t::no_memory), 0U, 0U};
            } else if ((flags & abi::vfs_open_trunc) != 0U) {
                sys::vfs::ramfs::truncate(sys::vfs::ramfs::files[ramfs_index]);
            }
            table[slot] = {handle_kind::ramfs_file, {}, ramfs_index, 0U};
            if ((flags & abi::vfs_open_append) != 0U)
                table[slot].position = sys::vfs::ramfs::files[ramfs_index].size;
            return {success, static_cast<sys::word_t>(slot), 0U};
        }

        if (!ext2_mounted)
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        sys::u32 inode_number = 0U;
        sys::ext2::inode node{};
        if (!sys::ext2::resolve_path(fs, path, inode_number, node))
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        const bool is_directory = (node.mode & sys::ext2::mode_type_mask) == sys::ext2::mode_directory;
        const bool is_regular = (node.mode & sys::ext2::mode_type_mask) == sys::ext2::mode_regular;
        if (!is_directory && !is_regular)
            return {static_cast<sys::word_t>(sys::error_t::unsupported), 0U, 0U};
        // ext2 is mounted read-only in this pass: opening it for anything
        // but reading is refused up front rather than accepted and failed
        // later on the first write.
        if (!is_directory && (flags & (abi::vfs_open_wronly | abi::vfs_open_rdwr)) != 0U)
            return {static_cast<sys::word_t>(sys::error_t::denied), 0U, 0U};
        table[slot] = {is_directory ? handle_kind::ext2_dir : handle_kind::ext2_file, node, -1, 0U};
        return {success, static_cast<sys::word_t>(slot), 0U};
    }

    handler_result do_read(sys::word_t handle, sys::word_t length) noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        if (!valid_handle(handle))
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        if (length > abi::vfs_buffer_size)
            length = abi::vfs_buffer_size;
        open_file& entry = table[handle];
        sys::usize_t produced = 0U;
        if (entry.kind == handle_kind::ext2_file) {
            produced = sys::ext2::read_file(fs, entry.node, entry.position, client_buffer(),
                                            static_cast<sys::usize_t>(length));
        } else if (entry.kind == handle_kind::ramfs_file) {
            produced = sys::vfs::ramfs::read_at(sys::vfs::ramfs::files[entry.ramfs_index],
                                                entry.position, client_buffer(),
                                                static_cast<sys::usize_t>(length));
        } else {
            return {static_cast<sys::word_t>(sys::error_t::unsupported), 0U, 0U};
        }
        entry.position += produced;
        return {success, static_cast<sys::word_t>(produced), 0U};
    }

    handler_result do_write(sys::word_t handle, sys::word_t length) noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        if (!valid_handle(handle))
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        if (length > abi::vfs_buffer_size)
            return {static_cast<sys::word_t>(sys::error_t::invalid_argument), 0U, 0U};
        open_file& entry = table[handle];
        if (entry.kind != handle_kind::ramfs_file)
            return {static_cast<sys::word_t>(sys::error_t::denied), 0U, 0U};
        const sys::usize_t written =
            sys::vfs::ramfs::write_at(sys::vfs::ramfs::files[entry.ramfs_index], entry.position,
                                      client_buffer(), static_cast<sys::usize_t>(length));
        entry.position += written;
        return {success, static_cast<sys::word_t>(written), 0U};
    }

    handler_result do_close(sys::word_t handle) noexcept {
        if (!valid_handle(handle))
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        table[handle] = {};
        return {static_cast<sys::word_t>(sys::error_t::success), 0U, 0U};
    }

    handler_result do_stat(sys::word_t path_length) noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        if (path_length == 0U || path_length >= abi::vfs_max_path)
            return {static_cast<sys::word_t>(sys::error_t::invalid_argument), 0U, 0U};
        char path[abi::vfs_max_path + 1U];
        (void)memcpy(path, client_buffer(), path_length);
        path[path_length] = '\0';

        if (under_tmp(path)) {
            const char* name = tmp_name(path);
            if (*name == '\0')
                return {success, 0U, static_cast<sys::word_t>(abi::vfs_file_type::directory)};
            const int index = sys::vfs::ramfs::find(name);
            if (index < 0)
                return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
            return {success, static_cast<sys::word_t>(sys::vfs::ramfs::files[index].size),
                   static_cast<sys::word_t>(abi::vfs_file_type::regular)};
        }

        if (!ext2_mounted)
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        sys::u32 inode_number = 0U;
        sys::ext2::inode node{};
        if (!sys::ext2::resolve_path(fs, path, inode_number, node))
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        const auto type = (node.mode & sys::ext2::mode_type_mask) == sys::ext2::mode_directory
                              ? abi::vfs_file_type::directory
                              : abi::vfs_file_type::regular;
        return {success, node.size, static_cast<sys::word_t>(type)};
    }

    struct readdir_state final {
        sys::word_t target_index;
        sys::word_t current_index;
        bool found;
        sys::u8 name_length;
        sys::u8 file_type;
    };

    [[nodiscard]] bool readdir_visitor(void* context, const char* name, sys::u8 name_length,
                                       sys::u32, sys::u8 file_type) noexcept {
        auto& state = *static_cast<readdir_state*>(context);
        if (state.current_index == state.target_index) {
            const sys::usize_t copy_length =
                name_length < abi::vfs_max_path ? name_length : abi::vfs_max_path;
            (void)memcpy(client_buffer(), name, copy_length);
            state.name_length = static_cast<sys::u8>(copy_length);
            state.file_type = file_type;
            state.found = true;
            return false;
        }
        ++state.current_index;
        return true;
    }

    handler_result do_readdir(sys::word_t handle, sys::word_t index) noexcept {
        if (!valid_handle(handle) || table[handle].kind != handle_kind::ext2_dir)
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        readdir_state state{index, 0U, false, 0U, 0U};
        if (!sys::ext2::list_directory(fs, table[handle].node, &readdir_visitor, &state))
            return {static_cast<sys::word_t>(sys::error_t::invalid_argument), 0U, 0U};
        if (!state.found)
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        return {static_cast<sys::word_t>(sys::error_t::success), state.name_length,
               state.file_type};
    }

    handler_result do_seek(sys::word_t handle, sys::word_t position) noexcept {
        if (!valid_handle(handle))
            return {static_cast<sys::word_t>(sys::error_t::not_found), 0U, 0U};
        table[handle].position = position;
        return {static_cast<sys::word_t>(sys::error_t::success), position, 0U};
    }

    [[nodiscard]] bool map_shared_frames() noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t read_write = static_cast<sys::word_t>(abi::CapabilityRight::read) |
                                       static_cast<sys::word_t>(abi::CapabilityRight::write);
        const sys::word_t attrs = abi::encode_mapping_attributes(
            abi::memory_type::normal, abi::memory_shareability::inner_shareable);
        if (sys::control(abi::control_operation::map_frame, native::own_space,
                         block_shared_frame_selector, block_scratch_address, read_write,
                         attrs) != success)
            return false;
        return sys::control(abi::control_operation::map_frame, native::own_space,
                            client_shared_frame_selector, client_scratch_address, read_write,
                            attrs) == success;
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    if (!map_shared_frames()) {
        native::signal_failure();
        return 1;
    }
    /*
     * A mount failure is not fatal to bring-up -- see ext2_mounted's
     * comment -- so it is not checked here at all; only the shared-frame
     * mappings above, without which VFS cannot serve anything, gate
     * readiness.
     */
    ext2_mounted = sys::ext2::mount(fs, &read_kib, nullptr);
    native::signal_ready(abi::vfs_service_ready_badge);

    for (;;) {
        const auto request = sys::ipc_receive(service_endpoint, abi::encode_timeout(1U));
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        const auto operation = static_cast<abi::vfs_operation>(request.message0);
        handler_result result{static_cast<sys::word_t>(sys::error_t::invalid_argument), 0U, 0U};
        switch (operation) {
            case abi::vfs_operation::open:
                result = do_open(request.message1, request.message2);
                break;
            case abi::vfs_operation::read:
                result = do_read(request.message1, request.message2);
                break;
            case abi::vfs_operation::write:
                result = do_write(request.message1, request.message2);
                break;
            case abi::vfs_operation::close:
                result = do_close(request.message1);
                break;
            case abi::vfs_operation::stat:
                result = do_stat(request.message1);
                break;
            case abi::vfs_operation::readdir:
                result = do_readdir(request.message1, request.message2);
                break;
            case abi::vfs_operation::seek:
                result = do_seek(request.message1, request.message2);
                break;
        }
        if (sys::ipc_reply(result.status, result.value1, result.value2, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
