# Release and compatibility policy

Zilch kernel releases use semantic versioning. The project version and native ABI version are related but independent: a kernel release may add implementation features without changing the ABI version.

## Release classes

- Patch releases fix defects and documentation without incompatible ABI or persisted diagnostic-format changes.
- Minor releases may add backward-compatible ABI operations, object types, event types, or optional features.
- Major releases may make incompatible product or ABI changes and require an explicit migration guide.

## Compatibility gates

Every release must pass:

- public ABI layout and header-self-containment checks;
- production source and ELF boundary checks;
- ARM64 certification and release builds;
- the production-readiness evidence applicable to completed requirements;
- changelog review for added, deprecated, or removed interfaces.

Certification-only ABIs, fixtures, and model controls are never product compatibility commitments.

## Deprecation

A product ABI operation can be deprecated only after a replacement exists and is documented. It remains supported for at least one subsequent minor release. The first deprecating release must name the replacement and planned removal release. Numeric identifiers are never reused.

## Diagnostic formats

Machine-readable diagnostic formats carry their own version. Backward-compatible event additions retain the format major version. Layout or semantic reinterpretation requires a new format version.
