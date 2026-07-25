#pragma once

#include <sys/abi/v1/syscall_numbers.hpp>
#include <sys/types.hpp>

extern "C" sys::word_t sys_invoke_raw(sys::word_t number,
                                      sys::word_t argument0,
                                      sys::word_t argument1,
                                      sys::word_t argument2,
                                      sys::word_t argument3) noexcept;
