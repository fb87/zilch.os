# Console service executable

The independently linked PL3 console service for control-plane role `0x202`.
Its private endpoint supports health, description, service-stop, string
write, single-byte write, and single-byte read.

The service owns no hardware. Physical PL011 ownership belongs to the serial
driver, and this service is a relay that forwards character I/O to it over
the driver's private operation set. Its own client-facing operation codes and
message layout are unchanged by that move, so existing callers -- root's boot
diagnostic and the domain manager's virtual PL011 emulation -- are unaffected
apart from which endpoint they address for reads.

Write and read are served by two independent threads rather than one loop.
The main thread serves writes and the lifecycle operations on the service
endpoint. A second thread serves reads on a separate, dedicated endpoint; the
service spawns it itself under a reserved role bound to this same executable,
and the entry point dispatches on the role argument to select which loop to
run. The two threads share a capability space but not memory, and share no
state: each forwards only its own operation type and replies, so no
coordination between them is required.

Spawning is ordered against root's minting, not assumed to follow it. Root
can only mint the read endpoint after process creation returns it a task
capability, so this service can and does begin running first. The service
therefore waits for that capability to resolve before creating the second
thread. Creating it earlier left the new thread failing capability
resolution immediately rather than blocking, spinning on the failure, and
starving its processor -- which prevented the rest of the service graph from
launching at all.

Splitting reads onto a separate endpoint means clients that consume input
must address that endpoint specifically. Root mints both capabilities into
clients that need receive as well as transmit.
