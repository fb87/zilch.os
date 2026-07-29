# Root resource manager

The production `init` image is the root resource manager. It launches the core
PL3 service roles in dependency order, retains their process-bundle
capabilities, consumes readiness/failure notifications, and remains resident as
the policy owner. Certification builds replace production policy with the
acceptance runner at a compile-time boundary.
