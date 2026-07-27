# Diagnostic record format

## Version

The in-kernel per-CPU record format is version 1. Consumers must reject unsupported versions and must read `sequence` with acquire ordering before consuming the remaining fields.

## Layout

Each record contains:

- `sequence`: nonzero per-CPU monotonic publication number;
- `version`: format version, currently 1;
- `kind`: stable event identifier;
- `cpu`: producing CPU;
- `argument[0..4]`: event-specific unsigned machine words.

Rings contain 32 records per CPU and overwrite the oldest record after wrapping. Producers fill every field and publish `sequence` last with a release store.

## Event identifiers

| ID | Event | Arguments |
|---:|---|---|
| 1 | exception entry | EL, vector, ESR, PC |
| 2 | printk contention | none |
| 3 | fatal exception | EL, vector, ESR, FAR, PC |
| 4 | stack corruption | EL |
| 5 | certification | test marker |
| 6 | IRQ | IRQ ID, vector, EL |
| 7 | scheduler switch | old thread, new thread, old generation, new generation |
| 8 | IPC | thread, operation, endpoint selector |
| 9 | VM exit | vCPU, reason, ESR, IPA/qualification, guest PC |
| 10 | user fault | thread, ESR, FAR, PC |

## Production policy

`CONFIG_TRACE=0` removes routine events 6 through 10 from release builds. Events 1 through 4 remain available for failure diagnosis. Release console messages omit guest registers and user/guest PC, FAR, ESR, and IPA values. Detailed records and console diagnostics are enabled only in development and certification variants.
