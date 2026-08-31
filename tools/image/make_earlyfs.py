#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Pack a set of named files into a ZEFS1 earlyfs image.

Format (see include/sys/platform/v1/earlyfs.hh for the authoritative
in-kernel/userspace reader):

    header_t   { magic[4]="ZEF1", u32 version=1, u32 entry_count, u32 reserved=0 }   16 bytes
    entry_t[]  { char name[48], u64 offset, u64 size }                              64 bytes each
    <page-aligned file data, one blob per entry, in entry order>

No timestamps, modes, checksums, or compression -- deliberately narrower than
tar/cpio, and fully reproducible: output depends only on entry names and file
contents, never on the host environment or SOURCE_DATE_EPOCH.
"""
import argparse
import struct
import sys

MAGIC = b"ZEF1"
VERSION = 1
NAME_SIZE = 48
HEADER_FORMAT = "<4sIII"
ENTRY_FORMAT = f"<{NAME_SIZE}sQQ"
PAGE_SIZE = 4096


def pad(size: int) -> int:
    return (PAGE_SIZE - (size % PAGE_SIZE)) % PAGE_SIZE


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--entry",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="add a named entry (repeatable); use '-' as PATH to skip",
    )
    parser.add_argument("--output", required=True, metavar="PATH")
    args = parser.parse_args()

    entries = []
    for raw in args.entry:
        if "=" not in raw:
            parser.error(f"--entry must be NAME=PATH, got {raw!r}")
        name, path = raw.split("=", 1)
        if path == "-":
            continue
        name_bytes = name.encode("utf-8")
        if len(name_bytes) >= NAME_SIZE:
            parser.error(f"entry name too long (max {NAME_SIZE - 1} bytes): {name!r}")
        with open(path, "rb") as handle:
            data = handle.read()
        entries.append((name_bytes, data))

    if len(entries) > 32:
        parser.error(f"too many entries ({len(entries)}, max 32)")

    header = struct.pack(HEADER_FORMAT, MAGIC, VERSION, len(entries), 0)
    directory_size = len(entries) * struct.calcsize(ENTRY_FORMAT)
    offset = len(header) + directory_size

    directory = b""
    blobs = b"\0" * pad(offset)
    offset += pad(offset)
    for name_bytes, data in entries:
        directory += struct.pack(ENTRY_FORMAT, name_bytes, offset, len(data))
        blobs += data
        offset += len(data)
        padding = pad(offset)
        blobs += b"\0" * padding
        offset += padding

    with open(args.output, "wb") as handle:
        handle.write(header)
        handle.write(directory)
        handle.write(blobs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
