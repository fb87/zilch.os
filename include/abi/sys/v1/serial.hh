#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    /*
     * Private wire protocol between console-server and the serial driver
     * (src/user/drivers/serial). Not control_plane_operation: the serial
     * driver is not a control_plane_role (it doesn't fit the fixed 5-slot
     * role/loop -- see root_graph.hh's memory-server precedent for a
     * service wired the same way), so it has no role-dispatch semantics to
     * share with that enum. Mirrors memory_server_operation's precedent of
     * a non-control-plane-role service defining its own small ABI enum.
     */
    enum class serial_operation : word_t {
        // NUL-terminated string, packed into message1..message3 the same
        // way control_plane_operation::write is -- see console_write_max_bytes.
        write = 0U,
        // Single byte in message1's low byte.
        write_byte = 1U,
        // No payload; replies message0 = 1 if a byte was available (0
        // otherwise), message1 = the byte.
        read_byte = 2U,
    };
} // namespace sys::abi::v1
