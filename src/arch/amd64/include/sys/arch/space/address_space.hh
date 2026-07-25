#pragma once
#include <sys/types.hh>
namespace sys::arch::space { inline constexpr bool user_available = false; struct address_space {}; inline void initialize(address_space&) noexcept {} inline void activate(address_space&) noexcept {} inline constexpr vaddr_t entry() noexcept { return 0U; } inline constexpr vaddr_t stack_top() noexcept { return 0U; } }
