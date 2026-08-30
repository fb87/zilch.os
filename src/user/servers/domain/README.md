# Domain service

Role `0x203` owns VM construction and assignment policy and depends on the
process service. This batch establishes its independent PL3 lifecycle, a
stable lifecycle wrapper, and a VM launch/destroy request path. A dedicated
load operation exists, but production management-policy attachment and real
guest deployment remain open.
