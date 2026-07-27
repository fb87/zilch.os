# Build and ABI Boundary Batch 0086

This batch replaces the top-level mixed build graph with non-recursive `.mk` ownership fragments.

- `src/kernel/kernel.mk` owns kernel compilation and linking.
- `src/user/user.mk` owns PL3 programs and verification guests.
- `src/image/image.mk` owns earlyfs and bootstrap packaging.
- `tests/tests.mk` owns certification-only data adapters.
- `mk/` owns shared configuration, toolchain selection, and checks.

Userspace sees only public ABI headers, its private `libsys` headers, and a dedicated certification ABI include tree. It cannot see production kernel or kernel-verification headers. Public ABI headers are compiled independently to enforce self-containment.
