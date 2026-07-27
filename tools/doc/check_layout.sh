#!/bin/sh
set -eu
root=${1:?repository root required}
allowed='README.md CHANGELOG.md'
for file in "$root"/*.md; do
    [ -e "$file" ] || continue
    name=$(basename "$file")
    case " $allowed " in
        *" $name "*) ;;
        *) echo "error: top-level documentation must live under docs/: $name" >&2; exit 1 ;;
    esac
done
[ -f "$root/docs/README.md" ] || { echo 'error: missing docs/README.md' >&2; exit 1; }
[ -f "$root/docs/readiness/PRODUCTION_READINESS_CHECKLIST.md" ] || { echo 'error: missing authoritative readiness checklist' >&2; exit 1; }
echo 'documentation layout: PASS'
