#!/usr/bin/env python3
"""Turn Chinese prose into a whole-sentence conversion eval set.

Reads text files, cuts them at punctuation into short Chinese-only sentences, reads each sentence back as unseparated quanpin using the shipped dictionary, and writes `pinyin<TAB>sentence` rows for `eval_sentences`.

Readings come from the dictionary itself by greedy longest match, so a sentence that contains a known phrase gets that phrase's pronunciation and most polyphones resolve correctly. The ones that do not (rare readings of 行, 得, 了 outside a known phrase) are noise, but they are the same noise for every configuration measured against the set, so comparisons stay valid.

Usage:
    build_eval_set.py --dictionary out/msime.db --out sentences.tsv docs/ README.md
"""

from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from pathlib import Path

# The dictionary stores phrases of up to 8 syllables (tbl_1..7 plus tbl_others), and one Han character is one syllable, so no lookup needs to reach further than this.
MAX_PHRASE_CHARS = 8

TEXT_SUFFIXES = {".md", ".txt", ".markdown"}

# Markdown and source noise that would otherwise become sentences: fenced blocks, inline code, links, HTML tags.
FENCE_RE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE_RE = re.compile(r"`[^`]*`")
LINK_RE = re.compile(r"\[([^\]]*)\]\([^)]*\)")
TAG_RE = re.compile(r"<[^>]+>")
URL_RE = re.compile(r"https?://\S+")


def is_han(ch: str) -> bool:
    return "一" <= ch <= "鿿"


def iter_text_files(inputs: list[str]) -> list[Path]:
    files: list[Path] = []
    for raw in inputs:
        path = Path(raw)
        if path.is_dir():
            files.extend(sorted(p for p in path.rglob("*") if p.suffix.lower() in TEXT_SUFFIXES))
        elif path.is_file():
            files.append(path)
        else:
            print(f"skipping {path}: not a file or directory", file=sys.stderr)
    return files


def extract_sentences(files: list[Path], low: int, high: int) -> list[str]:
    """Every maximal run of Han characters whose length falls in [low, high], in reading order, deduplicated."""
    seen: set[str] = set()
    sentences: list[str] = []
    for path in files:
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            print(f"skipping {path}: {error}", file=sys.stderr)
            continue
        in_fence = False
        for line in text.splitlines():
            if FENCE_RE.match(line):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            line = URL_RE.sub(" ", line)
            line = LINK_RE.sub(r"\1", line)
            line = INLINE_CODE_RE.sub(" ", line)
            line = TAG_RE.sub(" ", line)
            run: list[str] = []
            for ch in line + "\n":
                if is_han(ch):
                    run.append(ch)
                    continue
                if low <= len(run) <= high:
                    candidate = "".join(run)
                    if candidate not in seen:
                        seen.add(candidate)
                        sentences.append(candidate)
                run = []
    return sentences


def needed_substrings(sentences: list[str]) -> set[str]:
    """Only the substrings that some sentence could actually match, so the dictionary scan keeps a few thousand rows instead of 1.2 million."""
    wanted: set[str] = set()
    for sentence in sentences:
        for start in range(len(sentence)):
            for length in range(1, min(MAX_PHRASE_CHARS, len(sentence) - start) + 1):
                wanted.add(sentence[start : start + length])
    return wanted


def load_readings(database: Path, wanted: set[str]) -> dict[str, list[str]]:
    """value -> syllables, keeping the heaviest reading of each value.

    Rows whose syllable count differs from the character count are dropped: those are entries where a key does not line up with the text one syllable per character (latin mixed in, or a compressed key), and a misaligned reading would corrupt the pinyin of any sentence that used it.
    """
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)
    tables = [row[0] for row in connection.execute("select name from sqlite_master where type='table' and name like 'tbl_%'")]
    best: dict[str, tuple[int, list[str]]] = {}
    for table in tables:
        for key, value, weight in connection.execute(f"select key, value, weight from {table}"):  # noqa: S608 - table names come from sqlite_master
            if value not in wanted or not key:
                continue
            syllables = key.split("'")
            if len(syllables) != len(value) or not all(syllable.isalpha() for syllable in syllables):
                continue
            current = best.get(value)
            if current is None or weight > current[0]:
                best[value] = (weight or 0, syllables)
    connection.close()
    return {value: syllables for value, (_, syllables) in best.items()}


def read_sentence(sentence: str, readings: dict[str, list[str]]) -> list[str] | None:
    """Greedy longest match; None when some character has no reading at all."""
    syllables: list[str] = []
    position = 0
    while position < len(sentence):
        for length in range(min(MAX_PHRASE_CHARS, len(sentence) - position), 0, -1):
            found = readings.get(sentence[position : position + length])
            if found is not None:
                syllables.extend(found)
                position += length
                break
        else:
            return None
    return syllables


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inputs", nargs="+", help="text files or directories to read")
    parser.add_argument("--dictionary", required=True, type=Path, help="path to msime.db")
    parser.add_argument("--out", required=True, type=Path, help="TSV to write")
    parser.add_argument("--min", dest="low", type=int, default=3, help="shortest sentence in characters")
    parser.add_argument("--max", dest="high", type=int, default=20, help="longest sentence in characters")
    parser.add_argument("--limit", type=int, default=0, help="keep at most this many sentences (0 keeps all)")
    args = parser.parse_args()

    if not args.dictionary.is_file():
        print(f"no dictionary at {args.dictionary}", file=sys.stderr)
        return 1

    files = iter_text_files(args.inputs)
    if not files:
        print("no input files", file=sys.stderr)
        return 1
    sentences = extract_sentences(files, args.low, args.high)
    if not sentences:
        print("no sentences matched the length window", file=sys.stderr)
        return 1
    readings = load_readings(args.dictionary, needed_substrings(sentences))

    rows: list[tuple[str, str]] = []
    unread = 0
    for sentence in sentences:
        syllables = read_sentence(sentence, readings)
        if syllables is None:
            unread += 1
            continue
        rows.append(("".join(syllables), sentence))
        if args.limit and len(rows) >= args.limit:
            break

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8") as handle:
        for pinyin, sentence in rows:
            handle.write(f"{pinyin}\t{sentence}\n")

    characters = sum(len(sentence) for _, sentence in rows)
    print(f"{len(files)} files -> {len(sentences)} sentences, {len(rows)} written ({characters} characters), {unread} dropped for missing readings")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
