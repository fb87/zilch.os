# Root boot information

`bootinfo` is the versioned kernel-to-root-task contract. It describes online CPUs and the initial capability selectors installed in the root task's CSpace. The structure is kernel-owned and read-only to the initial task.
