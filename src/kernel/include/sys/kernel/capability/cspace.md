# Capability space

A CSpace is a fixed-size initial capability table. Invocation resolves a user
selector in the current task's CSpace, checks object type and rights, then
resolves the generation-tagged object reference through the object table.
