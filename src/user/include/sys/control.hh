#pragma once

#include <sys/syscall.hh>

#include <abi/sys/v1/control.hh>

namespace sys
{
    [[nodiscard]] inline word_t control(abi::v1::control_operation operation, word_t argument1 = 0U,
                                        word_t argument2 = 0U, word_t argument3 = 0U,
                                        word_t argument4 = 0U, word_t argument5 = 0U) noexcept {
        return invoke(abi::v1::syscall::control, static_cast<word_t>(operation), argument1,
                      argument2, argument3, argument4, argument5);
    }

    [[nodiscard]] inline word_t control_result1(word_t& result1,
                                                abi::v1::control_operation operation,
                                                word_t argument1 = 0U, word_t argument2 = 0U,
                                                word_t argument3 = 0U, word_t argument4 = 0U,
                                                word_t argument5 = 0U) noexcept {
        return sys_invoke_result1_raw(static_cast<word_t>(abi::v1::syscall::control),
                                      static_cast<word_t>(operation), argument1, argument2,
                                      argument3, argument4, argument5, &result1);
    }
} // namespace sys
