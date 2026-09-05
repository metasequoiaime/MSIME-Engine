#!/usr/bin/env python3
"""Check that every repository path NOTICE.md cites still exists.

NOTICE.md is the only record of where this data came from and under what terms, so a path that
quietly moves takes its attribution with it. Two had already drifted when this check was written:
`source/Wubi86.txt` had become `cn/Wubi86.txt` and `source/BaseDictIceEn.txt` had become
`en/BaseDictIceEn.txt`, so the notice pointed at nothing for the wubi table and the English word list.

Deliberately removed files are listed in ALLOWED_MISSING. They stay cited because the notice explains
why they are gone.

    python3 tools/verify_notice_paths.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NOTICE = ROOT / "NOTICE.md"

# Cited on purpose while absent from the tree. en/oaldpe.mdx is the commercial dictionary body the
# notice records as removed.
ALLOWED_MISSING = {"en/oaldpe.mdx"}

# Paths appear as inline code spans. Directories are cited with a trailing slash.
CITATION = re.compile(r"`((?:cn|en|source|kaomoji|emoji|symbols|mix|makecikudb|tools)/[A-Za-z0-9_./-]*)`")


def main() -> int:
    missing = []
    for path in sorted(set(CITATION.findall(NOTICE.read_text(encoding="utf-8")))):
        if path in ALLOWED_MISSING:
            continue
        if not (ROOT / path).exists():
            missing.append(path)

    if missing:
        print("NOTICE.md cites paths that do not exist:", file=sys.stderr)
        for path in missing:
            print(f"  {path}", file=sys.stderr)
        print("Update the notice, or add the path to ALLOWED_MISSING with the reason.", file=sys.stderr)
        return 1

    print("NOTICE.md paths all resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
