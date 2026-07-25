# Userspace architecture

Zilch keeps policy outside the privileged L4 microkernel. The kernel creates the
initial address space and starts `/bin/init` from earlyfs. The root task then
launches memory, process, device, console, supervision, and domain-management
servers using capabilities delegated by the kernel.

The initial userspace build currently produces a freestanding `init.elf` and an
earlyfs archive. Loading the ELF and entering PL3 are planned kernel milestones;
the current boot marker remains kernel-only.

Dependency direction:

```
applications / personalities
          -> userspace services
          -> libsys
          -> include/sys/abi/v1
          -> native kernel syscall ABI
```

Userspace must not include kernel-private, architecture-native, or
platform-native headers.
