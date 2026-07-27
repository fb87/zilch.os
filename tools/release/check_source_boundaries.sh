#!/bin/sh
set -eu

root=${1:?source root required}
failed=0

for file in $(find "$root/include" "$root/src" "$root/tests" -type f \
    \( -name '*.hh' -o -name '*.cc' -o -name '*.S' \)); do
    lines=$(wc -l < "$file")
    case "$file" in
        *.hh) limit=1600 ;;
        *) limit=1200 ;;
    esac
    if [ "$lines" -gt "$limit" ]; then
        echo "source boundary: $file has $lines lines (limit $limit)" >&2
        failed=1
    fi
done

if rg -n '#include[[:space:]]*[<"].*(/tests/|tests/)' "$root/include" >/dev/null; then
    echo "source boundary: public headers may not depend on tests" >&2
    failed=1
fi

if rg -n '#include[[:space:]]*"\\.\\.' "$root/include" "$root/src" >/dev/null; then
    echo "source boundary: relative parent includes are forbidden" >&2
    failed=1
fi

if [ "$failed" -ne 0 ]; then
    exit 1
fi

echo "Source boundaries: PASS"
