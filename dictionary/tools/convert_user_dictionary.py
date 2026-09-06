#!/usr/bin/env python3
"""Convert a user dictionary from another input method into the Metasequoia import format.

Switching away from Sogou, Microsoft Pinyin or Rime means bringing years of accumulated words with
you, and the batch import in the settings window accepts exactly one shape: three Tab-separated
columns, `word<TAB>pinyin<TAB>weight`, with syllables separated by `'`. Until now the Windows guide
told people to paste the specification and their file into an AI and hope. That is the most common
obstacle for a new user and it deserves a real tool.

Supported inputs, all plain text:

    rime      A Rime dictionary (`*.dict.yaml`). Entries after the `...` separator, columns
              `word<TAB>code[<TAB>weight]`.
    csv       `word,pinyin` per line, which is what Sogou's own export produces.
    words     One word per line, no reading. Pinyin is generated with pypinyin.

Binary Sogou cell dictionaries (`.scel`) are deliberately not supported: exporting them to text from
Sogou first is both easier to verify and does not require this project to reimplement a proprietary
format from guesswork.

    python3 convert_user_dictionary.py --format rime  sogou.dict.yaml > out.txt
    python3 convert_user_dictionary.py --format words my_words.txt     > out.txt
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DEFAULT_WEIGHT = 1
# The importer requires full syllables, so a reading is only usable if it is made of them. This is
# the same alphabet the pinyin tables use; anything else (tone marks, digits, Zhuyin) is rejected
# rather than silently written out as a line the importer will refuse.
SYLLABLE = re.compile(r"^[a-z]+$")
HAN = re.compile(r"[㐀-䶿一-鿿豈-﫿]")


class ConversionError(ValueError):
    pass


def normalize_reading(reading: str, expected_syllables: int) -> str:
    """Return `ni'hao` form, or raise if the reading cannot be trusted.

    Sogou writes readings as an unseparated run (`nihao`) or apostrophe-separated. An unseparated
    run cannot be split reliably here -- `xian` is both `xi'an` and `xian` -- so it is only accepted
    when the word is a single character and no split is needed.
    """
    reading = reading.strip().lower().replace("’", "'")
    if not reading:
        raise ConversionError("empty reading")
    parts = [part for part in reading.split("'") if part]
    if not all(SYLLABLE.fullmatch(part) for part in parts):
        raise ConversionError(f"reading {reading!r} is not made of plain pinyin syllables")
    if len(parts) == expected_syllables:
        return "'".join(parts)
    if len(parts) == 1 and expected_syllables == 1:
        return parts[0]
    raise ConversionError(
        f"reading {reading!r} has {len(parts)} syllable(s) but the word has {expected_syllables} "
        "character(s); add ' between syllables"
    )


def readings_from_pypinyin(word: str) -> str:
    try:
        from pypinyin import Style, lazy_pinyin
    except ImportError as error:  # pragma: no cover - depends on the environment
        raise ConversionError(
            "generating pinyin needs pypinyin: pip install -r dictionary/requirements.txt"
        ) from error
    syllables = lazy_pinyin(word, style=Style.NORMAL, errors="ignore")
    if len(syllables) != len(word):
        raise ConversionError(f"pypinyin produced {len(syllables)} syllable(s) for {word!r}")
    return "'".join(syllables)


def parse_rime(lines: list[str]) -> list[tuple[str, str]]:
    """Entries follow the `...` separator; everything before it is YAML metadata."""
    try:
        start = next(index for index, line in enumerate(lines) if line.strip() == "...") + 1
    except StopIteration:
        raise ConversionError("no `...` separator found; is this a Rime dictionary?") from None
    entries = []
    for line in lines[start:]:
        line = line.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        columns = line.split("\t")
        if len(columns) < 2:
            raise ConversionError(f"expected at least two Tab-separated columns: {line!r}")
        entries.append((columns[0].strip(), columns[1].strip()))
    return entries


def parse_csv(lines: list[str]) -> list[tuple[str, str]]:
    entries = []
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        # Only the first comma separates; a word may legitimately contain one.
        word, separator, reading = line.partition(",")
        if not separator:
            raise ConversionError(f"expected `word,pinyin`: {line!r}")
        entries.append((word.strip(), reading.strip()))
    return entries


def parse_words(lines: list[str]) -> list[tuple[str, str]]:
    return [(line.strip(), "") for line in lines if line.strip() and not line.startswith("#")]


PARSERS = {"rime": parse_rime, "csv": parse_csv, "words": parse_words}


def convert(lines: list[str], source_format: str, weight: int = DEFAULT_WEIGHT) -> tuple[list[str], list[str]]:
    """Return (output lines, skipped-entry messages). Bad entries are reported, not fatal: a single
    unparseable line in a dictionary of thousands should not cost the user the whole conversion."""
    rows, skipped = [], []
    seen: set[tuple[str, str]] = set()
    for word, reading in PARSERS[source_format](lines):
        if not word:
            continue
        if not HAN.search(word):
            skipped.append(f"{word!r}: no Han characters; the quanpin table takes Chinese words")
            continue
        try:
            syllables = readings_from_pypinyin(word) if not reading else normalize_reading(reading, len(word))
        except ConversionError as error:
            skipped.append(f"{word!r}: {error}")
            continue
        if (word, syllables) in seen:
            continue
        seen.add((word, syllables))
        rows.append(f"{word}\t{syllables}\t{weight}")
    return rows, skipped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path, help="the file to convert")
    parser.add_argument("--format", required=True, choices=sorted(PARSERS), help="what the source is")
    parser.add_argument("--weight", type=int, default=DEFAULT_WEIGHT,
                        help=f"weight written for every entry (default: {DEFAULT_WEIGHT})")
    parser.add_argument("--output", type=Path, help="write here instead of standard output")
    arguments = parser.parse_args()

    if arguments.weight < 0:
        parser.error("weight must be a non-negative integer")
    lines = arguments.source.read_text(encoding="utf-8-sig").splitlines()
    try:
        rows, skipped = convert(lines, arguments.format, arguments.weight)
    except ConversionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    text = "\n".join(rows) + ("\n" if rows else "")
    if arguments.output:
        arguments.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)

    for message in skipped:
        print(f"skipped {message}", file=sys.stderr)
    print(f"converted {len(rows)} entries, skipped {len(skipped)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
