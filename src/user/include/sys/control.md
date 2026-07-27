# User control API

`sys::control()` invokes the kernel bootstrap capability-authorized control
plane. Arguments are selectors in the caller's CSpace, never raw kernel object
identifiers or addresses.
