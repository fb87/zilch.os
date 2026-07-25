# ADR-0001: Versioned architecture and platform contracts

Accepted. Major contract generations are represented by include-directory versions. Generic code uses `current.h`; backends implement an explicit version. Native backend headers cannot override canonical contract headers.
