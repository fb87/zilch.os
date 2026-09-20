# Formatted output

`printf`/`snprintf` over a shared conversion core writing either to a
descriptor or to a caller buffer.

The length modifier decides the `va_arg` type and getting it wrong is not
cosmetic: AAPCS64 leaves the upper bits of a promoted `int` unspecified, so
reading one as `long` yields whatever happened to be in the register above
the value. That showed up immediately as a mangled `%d`. Width and
zero-padding are supported; an unrecognised conversion is echoed rather than
swallowed, so a format bug is visible instead of silent.
