#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>
#include <abi/sys/v1/ipc.hh>
#include <abi/sys/v1/memory.hh>
#include <abi/sys/v1/message.hh>
#include <abi/sys/v1/object.hh>
#include <abi/sys/v1/syscall_numbers.hh>
#include <abi/sys/v1/version.hh>
#include <cstddef>
#include <type_traits>

using namespace sys;
using namespace sys::abi::v1;
static_assert(major_version == 1U);
static_assert(sizeof(word_t) == 8U);
static_assert(sizeof(cap_slot_t) == 4U);
static_assert(std::is_same_v<std::underlying_type_t<control_operation>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<hypervisor_operation>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<ipc_operation>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<syscall>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<vm_exit_reason>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<guest_hypercall>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<memory_server_operation>, word_t>);
static_assert(std::is_same_v<std::underlying_type_t<memory_permission>, u8>);
static_assert(std::is_same_v<std::underlying_type_t<memory_type>, u8>);
static_assert(std::is_same_v<std::underlying_type_t<memory_shareability>, u8>);
static_assert(std::is_same_v<std::underlying_type_t<ObjectType>, u16>);
static_assert(std::is_same_v<std::underlying_type_t<CapabilityRight>, u32>);
static_assert(sizeof(MessageTag) == sizeof(word_t));
static_assert(alignof(Message) == alignof(word_t));
static_assert(offsetof(Message, words) == sizeof(MessageTag));
static_assert(sizeof(Message) == sizeof(word_t) * (message_register_count + 1U));
static_assert(sizeof(ipc_result) == sizeof(word_t) * 6U);
static_assert(offsetof(ipc_result, sender) == sizeof(word_t));
static_assert(sizeof(ipc_transfer) == 24U);
static_assert(alignof(ipc_transfer) == alignof(word_t));
static_assert(sizeof(ipc_timeout) == 16U);
static_assert(offsetof(ipc_timeout, ticks) == 0U);
static_assert(offsetof(ipc_timeout, enabled) == sizeof(u64));
static_assert(std::is_standard_layout_v<Message>);
static_assert(std::is_trivially_copyable_v<Message>);
static_assert(std::is_standard_layout_v<ipc_result>);
static_assert(std::is_trivially_copyable_v<ipc_result>);
static_assert(std::is_standard_layout_v<ipc_transfer>);
static_assert(std::is_trivially_copyable_v<ipc_transfer>);
static_assert(std::is_standard_layout_v<ipc_timeout>);
static_assert(std::is_trivially_copyable_v<ipc_timeout>);
static_assert(static_cast<word_t>(syscall::ipc) == 0U);
static_assert(static_cast<word_t>(syscall::control) == 1U);
static_assert(static_cast<word_t>(control_operation::thread_exit) == 42U);
static_assert(static_cast<word_t>(guest_hypercall::diagnostic) == 6U);
int main() {
    return 0;
}
