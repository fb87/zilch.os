# Kernel object table

The object table converts generation-tagged object references into live typed
objects. Reusing an object-table slot increments its generation, preventing a
stale capability from resolving to a newly allocated object.
