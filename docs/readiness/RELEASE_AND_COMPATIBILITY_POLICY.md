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

## Build configuration policy

The target build system has exactly two Kconfig profile presets: debug and
release. Debug enables tests, self-test ABIs, verbose diagnostics, tracing, and
debug information. Release forces those mechanisms off and uses optimized
product compiler flags. The detailed configuration contract is defined in
`docs/architecture/BUILD_CONFIGURATION.md`.

Every release is built from the checked-in release defconfig. A release is
invalid if its generated configuration enables debug, tests, self-tests,
hypervisor self-tests, verbose diagnostics, trace, or debug information. Source
and ELF gates independently reject test/debug markers, embedded verification
guests, and DWARF sections.

Guest sample toolchains are not release dependencies. A clean release build
must succeed without fetching Zephyr, Linux, FreeBSD, or another external guest
source. Product guest assets must cross the generic external-image boundary and
must be versioned as release inputs when included.

## Deprecation

A product ABI operation can be deprecated only after a replacement exists and is documented. It remains supported for at least one subsequent minor release. The first deprecating release must name the replacement and planned removal release. Numeric identifiers are never reused.

## Diagnostic formats

Machine-readable diagnostic formats carry their own version. Backward-compatible event additions retain the format major version. Layout or semantic reinterpretation requires a new format version.
