"""Validate importable professional dictionary packs.

The checks mirror the Settings dictionary import rules closely enough to catch
bad pull requests before users discover them at import time.  Within the Engine dictionary tree, the validator also checks each
Chinese character pronunciation and detects entries already present upstream.
"""

from __future__ import annotations

from collections import defaultdict
from pathlib import Path
import re
import sys


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
PACKS_ROOT = REPOSITORY_ROOT / "packs"
PARENT_DICTIONARY_ROOT = REPOSITORY_ROOT.parent
HAN_RE = re.compile(r"[\u4e00-\u9fff]")
PINYIN_RE = re.compile(r"[a-z]+(?:'[a-z]+)*")
ENGLISH_KEY_RE = re.compile(r"[a-z]+(?:[-'][a-z]+)*")


def data_lines(path: Path, *, comments: bool) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    text = path.read_text(encoding="utf-8-sig")
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        if comments and line.startswith("#"):
            continue
        result.append((line_number, line))
    return result


def load_character_pronunciations() -> dict[str, set[str]]:
    source = PARENT_DICTIONARY_ROOT / "cn" / "SingleCharsAllV1.txt"
    pronunciations: dict[str, set[str]] = defaultdict(set)
    if not source.exists():
        return pronunciations
    for _, line in data_lines(source, comments=True):
        fields = line.split("\t")
        if len(fields) >= 2:
            pronunciations[fields[0]].add(fields[1])
    return pronunciations


def validate_quanpin(
    path: Path, pronunciations: dict[str, set[str]]
) -> tuple[list[str], set[tuple[str, str]], set[str]]:
    errors: list[str] = []
    entries: set[tuple[str, str]] = set()
    words: set[str] = set()
    for line_number, line in data_lines(path, comments=False):
        fields = line.split("\t")
        if len(fields) != 3:
            errors.append(f"{path}:{line_number}: expected word<TAB>pinyin<TAB>weight")
            continue
        word, pinyin, weight = (field.strip() for field in fields)
        syllables = pinyin.split("'")
        han_characters = HAN_RE.findall(word)
        if not word:
            errors.append(f"{path}:{line_number}: empty word")
        if not PINYIN_RE.fullmatch(pinyin):
            errors.append(f"{path}:{line_number}: invalid full pinyin {pinyin!r}")
        if not weight.isdigit():
            errors.append(f"{path}:{line_number}: weight must be a non-negative integer")
        if len(han_characters) != len(syllables):
            errors.append(
                f"{path}:{line_number}: {len(han_characters)} Han characters but "
                f"{len(syllables)} pinyin syllables"
            )
        if pronunciations and len(han_characters) == len(syllables):
            for character, syllable in zip(han_characters, syllables):
                allowed = pronunciations.get(character)
                if allowed and syllable not in allowed:
                    errors.append(
                        f"{path}:{line_number}: {character!r} is not annotated as {syllable!r} "
                        f"in SingleCharsAllV1.txt"
                    )
        entry = (word, pinyin)
        if entry in entries:
            errors.append(f"{path}:{line_number}: duplicate candidate {entry!r}")
        entries.add(entry)
        words.add(word)
    return errors, entries, words


def validate_english(path: Path) -> tuple[list[str], set[tuple[str, str]], set[str]]:
    errors: list[str] = []
    entries: set[tuple[str, str]] = set()
    displays: set[str] = set()
    for line_number, line in data_lines(path, comments=False):
        fields = line.split("\t")
        if len(fields) not in (2, 3):
            errors.append(f"{path}:{line_number}: expected key<TAB>display<TAB>weight")
            continue
        key, display = (field.strip() for field in fields[:2])
        weight = fields[2].strip() if len(fields) == 3 else "0"
        if not ENGLISH_KEY_RE.fullmatch(key):
            errors.append(f"{path}:{line_number}: invalid lowercase English key {key!r}")
        if not display:
            errors.append(f"{path}:{line_number}: empty display")
        if not weight.isdigit():
            errors.append(f"{path}:{line_number}: weight must be a non-negative integer")
        entry = (key, display)
        if entry in entries:
            errors.append(f"{path}:{line_number}: duplicate candidate {entry!r}")
        entries.add(entry)
        displays.add(display)
    return errors, entries, displays


def validate_translations(path: Path) -> tuple[list[str], set[str]]:
    errors: list[str] = []
    sources: set[str] = set()
    for line_number, line in data_lines(path, comments=True):
        fields = line.split("\t")
        if len(fields) != 2:
            errors.append(f"{path}:{line_number}: expected source<TAB>gloss")
            continue
        source, gloss = (field.strip() for field in fields)
        if not source or not gloss:
            errors.append(f"{path}:{line_number}: source and gloss must be non-empty")
        if source in sources:
            errors.append(f"{path}:{line_number}: duplicate translation source {source!r}")
        sources.add(source)
    return errors, sources


def find_base_overlaps(entries: set[tuple[str, str]]) -> set[tuple[str, str]]:
    overlaps: set[tuple[str, str]] = set()
    for filename in ("BaseDictAllV1Part1.txt", "BaseDictAllV1Part2.txt"):
        source = PARENT_DICTIONARY_ROOT / "cn" / filename
        if not source.exists():
            continue
        for _, line in data_lines(source, comments=True):
            fields = line.split("\t")
            if len(fields) >= 2 and (fields[0], fields[1]) in entries:
                overlaps.add((fields[0], fields[1]))
    return overlaps


def validate_pack(pack: Path, pronunciations: dict[str, set[str]]) -> list[str]:
    required = {
        "quanpin.txt": pack / "quanpin.txt",
        "english.txt": pack / "english.txt",
        "translations.txt": pack / "translations.txt",
        "README.md": pack / "README.md",
    }
    missing = [name for name, path in required.items() if not path.exists()]
    if missing:
        return [f"{pack}: missing required files: {', '.join(missing)}"]

    pinyin_errors, pinyin_entries, chinese_words = validate_quanpin(
        required["quanpin.txt"], pronunciations
    )
    english_errors, english_entries, english_displays = validate_english(
        required["english.txt"]
    )
    translation_errors, translation_sources = validate_translations(
        required["translations.txt"]
    )
    errors = pinyin_errors + english_errors + translation_errors

    for word in sorted(chinese_words - translation_sources):
        errors.append(f"{pack}: Chinese candidate has no translation: {word!r}")
    for display in sorted(english_displays - translation_sources):
        errors.append(f"{pack}: English candidate has no translation: {display!r}")
    for word, pinyin in sorted(find_base_overlaps(pinyin_entries)):
        errors.append(f"{pack}: candidate already exists in base dictionary: {word}\t{pinyin}")

    print(
        f"{pack.name}: {len(pinyin_entries)} Chinese candidates, "
        f"{len(english_entries)} English candidates, {len(translation_sources)} translations"
    )
    return errors


def main() -> int:
    if not PACKS_ROOT.exists():
        print(f"Pack directory does not exist: {PACKS_ROOT}", file=sys.stderr)
        return 1
    pronunciations = load_character_pronunciations()
    errors: list[str] = []
    root_translations = REPOSITORY_ROOT / "translations.txt"
    if root_translations.exists():
        root_errors, root_sources = validate_translations(root_translations)
        errors.extend(root_errors)
        print(f"root translations: {len(root_sources)} entries")
    packs = sorted(path for path in PACKS_ROOT.iterdir() if path.is_dir())
    if not packs:
        print(f"No packs found in {PACKS_ROOT}", file=sys.stderr)
        return 1
    for pack in packs:
        errors.extend(validate_pack(pack, pronunciations))
    if errors:
        print("\nValidation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    if not pronunciations:
        print("Validation OK (base-dictionary pronunciation checks skipped)")
    else:
        print("Validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
