#include <sys/arch/syscall.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/hypervisor.hh>
#include <abi/sys/v1/ipc.hh>
#include <abi/sys/v1/syscall.hh>

extern "C" sys::word_t sys_invoke_raw(sys::word_t number, sys::word_t argument0,
                                      sys::word_t argument1, sys::word_t argument2,
                                      sys::word_t argument3, sys::word_t argument4,
                                      sys::word_t argument5) noexcept {
    return sys::arch::syscall::invoke_raw(number, argument0, argument1, argument2, argument3,
                                          argument4, argument5);
}

extern "C" sys::word_t sys_ipc_invoke_raw(sys::word_t endpoint, sys::word_t operation,
                                          sys::word_t message0, sys::word_t message1,
                                          sys::word_t message2, sys::word_t message3,
                                          sys::word_t transfer_descriptor,
                                          sys::word_t timeout_descriptor) noexcept {
    return sys::arch::syscall::ipc_invoke_raw(endpoint, operation, message0, message1, message2,
                                              message3, transfer_descriptor, timeout_descriptor);
}

extern "C" sys::word_t sys_invoke_result1_raw(sys::word_t number, sys::word_t argument0,
                                              sys::word_t argument1, sys::word_t argument2,
                                              sys::word_t argument3, sys::word_t argument4,
                                              sys::word_t argument5,
                                              sys::word_t* result1) noexcept {
    return sys::arch::syscall::invoke_result1_raw(number, argument0, argument1, argument2,
                                                  argument3, argument4, argument5, result1);
}

sys::abi::v1::ipc_result sys_ipc_exchange_raw(sys::word_t endpoint, sys::word_t operation,
                                              sys::word_t message0, sys::word_t message1,
                                              sys::word_t message2, sys::word_t message3,
                                              sys::word_t transfer_descriptor,
                                              sys::word_t timeout_descriptor) noexcept {
    return sys::arch::syscall::ipc_exchange_raw(endpoint, operation, message0, message1, message2,
                                                message3, transfer_descriptor, timeout_descriptor);
}

sys::abi::v1::vm_exit_result sys_hypervisor_invoke_raw(sys::word_t operation, sys::word_t selector,
                                                       sys::word_t argument0, sys::word_t argument1,
                                                       sys::word_t argument2) noexcept {
    return sys::arch::syscall::hypervisor_invoke_raw(operation, selector, argument0, argument1,
                                                     argument2);
}
