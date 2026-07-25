#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

EXTS = {'.hh', '.cc', '.S'}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', type=Path, default=Path('src'))
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--index', type=Path, required=True)
    args = ap.parse_args()
    modules=[]
    for src in sorted(p for p in args.root.rglob('*') if p.suffix in EXTS):
        base=src.with_suffix('')
        doc=base.with_suffix('.md')
        tests=sorted(src.parent.glob(base.name + '*.tt'))
        modules.append({'source':str(src),'document':str(doc) if doc.exists() else None,'tests':[str(t) for t in tests]})
    missing=[m['source'] for m in modules if not m['document']]
    if missing:
        raise SystemExit('missing module documents:\n'+'\n'.join(missing))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w') as out:
        out.write('# Zilch collected module design\n\n')
        for m in modules:
            doc=Path(m['document'])
            out.write(f'\n---\n\n_Source: `{m["source"]}`_\n\n')
            out.write(doc.read_text())
            out.write('\n')
    args.index.write_text(json.dumps(modules, indent=2)+'\n')
    return 0
if __name__ == '__main__': raise SystemExit(main())
