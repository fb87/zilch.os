#pragma once

#include <sys/types.hh>

#include <abi/sys/v1/syscall_numbers.hh>

extern "C" sys::word_t sys_invoke_raw(sys::word_t number, sys::word_t argument0,
                                      sys::word_t argument1, sys::word_t argument2,
                                      sys::word_t argument3, sys::word_t argument4,
                                      sys::word_t argument5) noexcept;

extern "C" sys::word_t sys_invoke_result1_raw(sys::word_t number, sys::word_t argument0,
                                              sys::word_t argument1, sys::word_t argument2,
                                              sys::word_t argument3, sys::word_t argument4,
                                              sys::word_t argument5, sys::word_t* result1) noexcept;
