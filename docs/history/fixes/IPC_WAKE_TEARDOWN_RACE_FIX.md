# IPC wake versus teardown race fix

A blocked thread may be selected for destruction while another CPU still holds a reply or cancellation path that observed the old blocked state. The previous generic wake helper unconditionally changed every non-faulted/non-terminated state to `ready`, allowing a thread already published as `suspended` by teardown to be resurrected and scheduled while its task, CSpace, address space, and saved context were reclaimed.

Wake is now an atomic conditional transition from one of the four blocking states to `ready`. It cannot transition `suspended`, `terminated`, `faulted`, `inactive`, `ready`, or `running`. Thread resume remains an explicit control operation. This closes reply-versus-destroy and cancel-versus-destroy resurrection races without weakening remote-CPU quiescence.
