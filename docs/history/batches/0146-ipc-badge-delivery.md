# Batch 0146 — IPC endpoint badge delivery

- Snapshot the badge from the exact endpoint capability used to call.
- Deliver badges through both direct and queued rendezvous paths, including
  fault IPC, while keeping thread identity private in reply authority.
- Mint generation-tagged endpoint capabilities for dynamically created tasks.
- Preserve accepted-message identity when the invoking capability is later
  deleted or revoked.
- Certify badge delivery, rights attenuation, identity hiding, and
  post-accept authority deletion.
