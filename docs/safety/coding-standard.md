# Restricted C++20 Profile

Allowed: namespaces, constexpr/consteval, static_assert, enum class, trivial structs, bounded templates, explicit initialization, noexcept.

Forbidden: exceptions, RTTI, virtual dispatch, inheritance by default, coroutines, implicit allocation, hosted STL, global dynamic initialization, unbounded recursion, and hidden blocking.
