# Scheduling-context budget and lock evidence

Each context carries a bounded budget and period. Exhausted contexts are
replenished at the next validated period deadline; budget and period updates
reject zero values, budget greater than period, and deadline overflow.

Donation remains separate and is consumed through the existing authority
chain. True per-slice sporadic-server replenishment remains SCH-010 work.

Every ranked kernel lock records its acquisition counter and updates a
generation-safe maximum on release. Certification reports the maximum hold
duration in architectural timer ticks and rejects lock-order violations.
IRQ-disabled-section timing remains SCH-017 work. Hardware-specific latency
targets remain outside this bounded evidence.
