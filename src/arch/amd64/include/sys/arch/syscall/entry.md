# Module: amd64 syscall/entry

Architecture-specific implementation of syscall/entry. Generic kernel code consumes this through the selected architecture include path and does not reference architecture privilege-level registers or frame layouts directly.
