# Batch 0154: reproducible release builds

## Scope

Close TST-036 by making release artifacts deterministic and adding a two-build
verification target for both supported architectures.

## Evidence

- `make BUILD_VARIANT=release reproducible-check`
- ARM64 and AMD64 release ELF, raw image, userspace ELF/map, and earlyfs
  artifacts match across clean builds with `SOURCE_DATE_EPOCH=0`.
- Early filesystem tar creation now fixes ordering, timestamps, and numeric
  ownership so archive bytes do not depend on the temporary staging directory.

Linker map files contain absolute output paths, so those paths are normalized
only for the map comparison; executable and archive artifacts remain strict
byte-for-byte comparisons.
