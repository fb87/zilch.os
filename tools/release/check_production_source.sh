#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_production_source.sh <source-root>")

root = pathlib.Path(sys.argv[1]).resolve()
scopes = [root / "src", root / "include"]
requirements = {
    "acceptance_report": {"CONFIG_SELFTEST"},
    "acceptance_finalize": {"CONFIG_SELFTEST"},
    "acceptance_worker_tick": {"CONFIG_SELFTEST"},
    "acceptance_query": {"CONFIG_SELFTEST"},
    "hypervisor_self_test": {"CONFIG_SELFTEST", "CONFIG_HYPERVISOR_SELFTEST"},
}
source_suffixes = {".cc", ".hh", ".h", ".S", ".c", ".cpp", ".hpp"}

conditional = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")
errors: list[str] = []

for scope in scopes:
    for path in sorted(scope.rglob("*")):
        if not path.is_file() or path.suffix not in source_suffixes:
            continue
        stack: list[set[str]] = []
        for lineno, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            match = conditional.match(line)
            if match:
                directive, expression = match.groups()
                expression = expression.strip()
                if directive in {"if", "ifdef", "ifndef"}:
                    names = set(re.findall(r"CONFIG_[A-Z0-9_]+", expression))
                    stack.append(names)
                elif directive == "elif":
                    if not stack:
                        errors.append(f"{path}:{lineno}: unmatched #elif")
                    else:
                        stack[-1] = set(re.findall(r"CONFIG_[A-Z0-9_]+", expression))
                elif directive == "else":
                    # Preserve the controlling configuration name. The else arm is
                    # still configuration-scoped even though the polarity changes.
                    if not stack:
                        errors.append(f"{path}:{lineno}: unmatched #else")
                elif directive == "endif":
                    if not stack:
                        errors.append(f"{path}:{lineno}: unmatched #endif")
                    else:
                        stack.pop()
                continue

            active_names = set().union(*stack) if stack else set()
            for token, allowed_configs in requirements.items():
                if token in line and not (active_names & allowed_configs):
                    expected = " or ".join(sorted(allowed_configs))
                    errors.append(
                        f"{path}:{lineno}: {token} is not guarded by {expected}"
                    )

        if stack:
            errors.append(f"{path}: unterminated preprocessor conditional")

if errors:
    print("error: production source gate failed", file=sys.stderr)
    for error in errors:
        print(error, file=sys.stderr)
    raise SystemExit(1)
