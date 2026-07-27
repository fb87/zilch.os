# Pager client executable

The pager client is a separate ELF image used by two independently created PL3
processes. Each process faults on the same virtual address in its own address
space, verifies private writable storage, calls the memory server, and reports a
role-derived completion badge. The root task destroys the first client and then
reuses the process slot for the second client, exercising image selection,
address-space isolation, and lifecycle generation checks.
