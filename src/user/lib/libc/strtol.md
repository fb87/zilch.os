# strtol

Standard string-to-long conversion: optional whitespace, optional sign, base
prefix detection for base 0 and base 16, digits in any base up to 36, and
`endptr` left at the first unconsumed character.

Accumulation is checked for overflow as it goes and saturates at
`LONG_MIN`/`LONG_MAX` with `errno` set, rather than wrapping — the callers
are parsing input this system did not produce.
