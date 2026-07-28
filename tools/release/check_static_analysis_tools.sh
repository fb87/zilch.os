#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

report=${1:-out/reports/static-analysis-tools.txt}
mkdir -p "$(dirname "$report")"
{
    echo "Static analysis tool inventory"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    for tool in scan-build clang-tidy; do
        if command -v "$tool" >/dev/null 2>&1; then
            printf '%s=%s\n' "$tool" "$(command -v "$tool")"
            "$tool" --version 2>&1 | head -1 | sed 's/^/version=/'
        else
            printf '%s=UNAVAILABLE\n' "$tool"
        fi
    done
    echo "status=DEVIATION_DOCUMENTED"
    echo "note=Install scan-build and clang-tidy, then run the project-specific profiles before release sign-off."
} >"$report"
cat "$report"
