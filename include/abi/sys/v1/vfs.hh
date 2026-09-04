#pragma once

#include <sys/types.hh>

/*
 * Private wire protocol between clients and the VFS server
 * (src/user/servers/vfs), same non-control-plane-role convention as
 * serial_operation and block_operation: the VFS server does not fit the
 * fixed 5-slot control_plane_role enum, so it defines its own small ABI
 * enum.
 *
 * Path and read/write payload bytes travel through one shared frame every
 * VFS client is minted alongside vfs_service_endpoint (see root_graph.hh's
 * spawn()), the same shape block_operation's shared payload frame uses --
 * a single global buffer is safe here for the same reason it is safe
 * there: every request is a synchronous IPC call into the VFS server's
 * single-threaded receive loop, so there is never more than one request's
 * bytes in the buffer at a time.
 */
namespace sys::abi::v1
{
    enum class vfs_operation : word_t {
        // message1 = path length in the shared buffer (bytes, from offset
        // 0); message2 = flags (see vfs_open_flags). Replies message0 =
        // status, message1 = handle.
        open = 0U,
        // message1 = handle; message2 = requested length, clamped to
        // vfs_buffer_size. Replies message0 = status, message1 = bytes
        // actually read; the bytes themselves land in the shared buffer.
        read = 1U,
        // message1 = handle; message2 = length already placed in the
        // shared buffer by the caller. Replies message0 = status,
        // message1 = bytes actually written.
        write = 2U,
        // message1 = handle. Replies message0 = status.
        close = 3U,
        // message1 = path length, same convention as open. Replies
        // message0 = status, message1 = size in bytes, message2 =
        // vfs_file_type.
        stat = 4U,
        // message1 = a handle opened on a directory; message2 = a 0-based
        // entry index. Replies message0 = status (not_found once index is
        // past the last entry), message1 = name length, message2 = a
        // vfs_file_type; the name bytes land in the shared buffer.
        // Re-walks the directory from the start each call rather than
        // holding cursor state server-side -- directories here are small
        // (earlyfs-scale binaries, not a general filesystem), so the
        // repeated O(n) walk costs nothing a caller would notice.
        readdir = 5U,
        // message1 = handle; message2 = new absolute position. Replies
        // message0 = status, message1 = the resulting position.
        seek = 6U,
    };

    inline constexpr usize_t vfs_buffer_size = 4096U;
    inline constexpr usize_t vfs_max_path = 255U;
    inline constexpr word_t vfs_max_open_files = 32U;

    // Matches unistd.h's O_* bits (io.cc), which is the only caller.
    inline constexpr word_t vfs_open_rdonly = 0U;
    inline constexpr word_t vfs_open_wronly = 1U;
    inline constexpr word_t vfs_open_rdwr = 2U;
    inline constexpr word_t vfs_open_creat = 0x40U;
    inline constexpr word_t vfs_open_trunc = 0x200U;
    inline constexpr word_t vfs_open_append = 0x400U;

    enum class vfs_file_type : word_t {
        unknown = 0U,
        regular = 1U,
        directory = 2U,
    };
} // namespace sys::abi::v1
