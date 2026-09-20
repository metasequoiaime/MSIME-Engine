#!/usr/bin/env python3
"""Check that known segmentation fragments stay out of the shipped Chinese dictionaries.

A fragment is a span cut out of a longer word by the corpus tokenizer -- `米高` from 「一米高」,
`钟的` from 「一分钟的」, `之事` from 「不平之事」. It is not a word, but it arrives carrying the
summed count of every phrase it was cut from, so it outranks real words and lands on the first
candidate page. `米高` reached 50215, eight times the weight of `米糕`, and on a nine-key layout
`64426` matches both `ni'hao` and `mi'gao` -- which is how it ended up third behind 「你好」.

Nothing in the build rejects these, because a fragment is well-formed: a plausible reading, a
plausible weight, a real pair of characters. The only signal that it is not a word is that upstream
rime-ice never had it. tools/segmentation_fragments.txt records the ones found so far along with the
longer words they were cut from; this check keeps them from coming back.

    python3 tools/verify_no_fragments.py
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FRAGMENTS = ROOT / "tools/segmentation_fragments.txt"

# The files a fragment has to be absent from: the ice source and both tables derived from it.
# BaseDictAllV1Part2.txt holds none of them today, and is checked anyway so a future re-split of the
# two parts cannot move a fragment into the unchecked half.
TABLES = (
    "source/BaseDictIce.txt",
    "cn/BaseDictIceV1.txt",
    "cn/BaseDictAllV1Part1.txt",
    "cn/BaseDictAllV1Part2.txt",
)

# Below this a fragment cannot reach the first candidate page, and the entry is indistinguishable
# from the long tail of rare-but-real words the dictionary is allowed to carry. The point of the
# check is candidate placement, not purity.
VISIBLE_WEIGHT = 10000


def read_fragments() -> set[str]:
    words = set()
    for line in FRAGMENTS.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            words.add(line.split("\t")[0].split()[0])
    return words


def offending_entries(path: Path, fragments: set[str]) -> list[str]:
    found = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        # The ice source separates with spaces, the derived tables with tabs.
        fields = line.split("\t") if "\t" in line else line.split(" ")
        if len(fields) < 2 or not fields[-1].isdigit():
            continue
        if fields[0] in fragments and int(fields[-1]) >= VISIBLE_WEIGHT:
            found.append(line)
    return found


def main() -> int:
    fragments = read_fragments()
    if not fragments:
        print(f"{FRAGMENTS.relative_to(ROOT)} lists no fragments", file=sys.stderr)
        return 1
    failures = 0
    for name in TABLES:
        path = ROOT / name
        if not path.is_file():
            print(f"missing table: {name}", file=sys.stderr)
            failures += 1
            continue
        for entry in offending_entries(path, fragments):
            print(f"{name}: segmentation fragment back at a visible weight: {entry}", file=sys.stderr)
            failures += 1
    if failures:
        print(
            f"\n{failures} fragment entries found. They are listed in "
            f"{FRAGMENTS.relative_to(ROOT)} with the longer words they were cut from.",
            file=sys.stderr,
        )
        return 1
    print(f"no segmentation fragments at or above weight {VISIBLE_WEIGHT} in {len(TABLES)} tables")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
