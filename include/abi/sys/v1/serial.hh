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
     *
     * The operations below are split across TWO endpoints, not one: write/
     * write_byte go to root_graph.hh's serial_service_endpoint, read_byte/
     * read_byte_wait to its serial_rx_endpoint, each served by its own
     * thread in the driver. That is a correctness requirement, not a
     * layering preference -- read_byte_wait defers its reply, and a deferred
     * reply is silently destroyed by the next call to reach the same thread.
     * See serial_rx_endpoint's comment for the full argument.
     */
    enum class serial_operation : word_t {
        // NUL-terminated string, packed into message1..message3 the same
        // way control_plane_operation::write is -- see console_write_max_bytes.
        write = 0U,
        // Single byte in message1's low byte.
        write_byte = 1U,
        // No payload; replies IMMEDIATELY, message0 = 1 if a byte was
        // available (0 otherwise), message1 = the byte. Non-blocking by
        // contract -- domain-manager's forward_console_input() depends on
        // this to check for guest RX input on a VM idle exit without
        // stalling that exit's handling (and therefore the guest's own
        // vcpu) when nothing has been typed. See read_byte_wait for the
        // blocking alternative.
        read_byte = 2U,
        // Same wire shape as read_byte, but replies only once a byte is
        // actually available: a request that finds the ring empty is held
        // (the kernel keeps the caller's reply capability across the RX
        // thread's next ipc_receive(), same mechanism as any other deferred
        // reply) until a subsequent RX interrupt delivers one. Used by
        // console-server's stdin thread on behalf of the interactive
        // shell's read() (src/user/lib/libc/io.cc), which wants to block --
        // never by domain-manager, which must not.
        //
        // Holding that reply is only safe because of how the RX endpoint is
        // minted: console-server is its sole client and calls it from one
        // thread, so at most one reply is ever owed. See the enum comment.
        read_byte_wait = 3U,
    };
} // namespace sys::abi::v1
