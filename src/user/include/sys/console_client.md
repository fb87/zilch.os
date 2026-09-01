# Console client library

The client-side wrapper for the console service's IPC surface. It provides
string write, single-byte write, and single-byte read, and is header-only so
that callers link no additional objects.

String write packs a short NUL-terminated string into the message registers,
eight bytes per word. Text longer than the bounded capacity is truncated
rather than chunked; multi-call streaming is a later extension and no current
caller needs it. The single-byte variants exist for the domain manager's
virtual PL011 emulation, which forwards individual guest register accesses
rather than host diagnostic strings.

Every function takes the target endpoint as a parameter rather than assuming
a fixed capability slot, because callers hold their console capabilities at
different slots -- root and the domain manager each receive their own mints.

Read must be addressed to the console service's dedicated read endpoint, not
the endpoint used for writes. The service serves reads from a separate thread
on a separate endpoint, so the two are distinct capabilities even though both
refer to the same service. Passing the write endpoint to read is a caller
error that surfaces as an unserviced request rather than a compile failure,
so callers that consume input must be minted both capabilities.

Read reports availability and value separately: a reply indicating no
byte available is a normal, expected result for an idle console, not an
error.
