#pragma once

#include <sys/types.hh>

namespace sys::arch::v1
{
    struct irq_state_t {
        word_t value;
    };

    struct user_context_t {
        reg_t general[32];
        vaddr_t stack_pointer;
        vaddr_t instruction_pointer;
        word_t status;
    };

    struct page_attributes_t {
        bool readable;
        bool writable;
        bool executable;
        bool user;
        bool device;
    };

    struct exception_info_t {
        word_t cause;
        vaddr_t fault_address;
        word_t detail;
    };
} // namespace sys::arch::v1
