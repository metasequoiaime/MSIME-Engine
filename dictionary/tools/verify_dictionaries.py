#!/usr/bin/env python3
"""Check that the artifacts in out/ are complete enough to ship.

build_all.py reports which stages ran, but a stage can succeed and still leave a table empty
when an input file silently changes shape. This guards the release path: every shipping table
must exist and carry at least a floor number of rows.

The floors are deliberately well below the current counts so ordinary dictionary edits do not
trip them; they exist to catch a table that came out empty or nearly so.
"""

from __future__ import annotations

import sqlite3
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = REPO_ROOT / "out"
sys.path.insert(0, str(REPO_ROOT))
from dictionary_format import quanpin_tables

# (table, minimum rows) per database.
EXPECTED_TABLES = {
    "msime.db": [
        ("wubi86", 50_000),
        ("quick_parases", 1),
        ("japanese_lexicon", 5_000),
    ],
    "english.db": [
        ("english_words", 100_000),
        ("en_zh_glosses", 50_000),
        ("zh_en_glosses", 20_000),
    ],
    "others.db": [
        ("emoji", 1_000),
        ("emoji_pinyin", 1_000),
        ("kaomoji", 500),
        ("kaomoji_catalog", 500),
        ("symbol_catalog", 1_000),
    ],
}

# The quanpin entries are spread over tbl_{1..7,others}_{letter}; check the total instead.
QUANPIN_TABLE_COUNT = len(quanpin_tables())
QUANPIN_MINIMUM_ROWS = 1_000_000

# dict_japanese.dat has no schema to inspect, so check the magic header and a floor size.
JAPANESE_MODEL_MAGIC = b"MSJPDT1\0"
JAPANESE_MODEL_MINIMUM_BYTES = 32 * 1024 * 1024

# The model is derived from Mozc's OSS dictionary, so its notice has to ship alongside it.
# Losing this file would ship the model without its IPAdic / ICOT / Okinawa attribution.
MOZC_NOTICE_NAME = "mozc_dictionary_oss_README.txt"
MOZC_NOTICE_REQUIRED_TERMS = ("IPAdic", "ICOT", "Okinawa")

EXPECTED_CHECKSUM_ENTRIES = 5


def fail(message: str, failures: list[str]) -> None:
    print(f"FAIL {message}")
    failures.append(message)


def check_row_counts(failures: list[str]) -> None:
    for database, tables in EXPECTED_TABLES.items():
        path = OUT_DIR / database
        if not path.is_file():
            fail(f"{database} is missing", failures)
            continue
        connection = sqlite3.connect(path)
        try:
            present = {row[0] for row in connection.execute("select name from sqlite_master where type='table'")}
            for table, minimum in tables:
                if table not in present:
                    fail(f"{database}: table {table} is missing", failures)
                    continue
                count = connection.execute(f"select count(*) from {table}").fetchone()[0]
                if count < minimum:
                    fail(f"{database}: {table} has {count} rows, expected at least {minimum}", failures)
                else:
                    print(f"ok   {database}: {table} = {count}")
        finally:
            connection.close()


def check_quanpin(failures: list[str]) -> None:
    path = OUT_DIR / "msime.db"
    if not path.is_file():
        return
    connection = sqlite3.connect(path)
    try:
        names = [row[0] for row in connection.execute(
            "select name from sqlite_master where type='table' and name like 'tbl\\_%' escape '\\'"
        )]
        if len(names) != QUANPIN_TABLE_COUNT:
            fail(f"msime.db: found {len(names)} quanpin tables, expected {QUANPIN_TABLE_COUNT}", failures)
        total = sum(connection.execute(f"select count(*) from {name}").fetchone()[0] for name in names)
        if total < QUANPIN_MINIMUM_ROWS:
            fail(f"msime.db: quanpin rows total {total}, expected at least {QUANPIN_MINIMUM_ROWS}", failures)
        else:
            print(f"ok   msime.db: {len(names)} quanpin tables, {total} rows")
    finally:
        connection.close()


def check_japanese_model(failures: list[str]) -> None:
    path = OUT_DIR / "dict_japanese.dat"
    if not path.is_file():
        fail("dict_japanese.dat is missing", failures)
        return
    size = path.stat().st_size
    if size < JAPANESE_MODEL_MINIMUM_BYTES:
        fail(f"dict_japanese.dat is {size} bytes, expected at least {JAPANESE_MODEL_MINIMUM_BYTES}", failures)
        return
    with path.open("rb") as stream:
        magic = stream.read(len(JAPANESE_MODEL_MAGIC))
    if magic != JAPANESE_MODEL_MAGIC:
        fail(f"dict_japanese.dat starts with {magic!r}, expected {JAPANESE_MODEL_MAGIC!r}", failures)
        return
    print(f"ok   dict_japanese.dat = {size / 1048576:.1f} MB")


def check_mozc_notice(failures: list[str]) -> None:
    path = OUT_DIR / MOZC_NOTICE_NAME
    if not path.is_file():
        fail(f"{MOZC_NOTICE_NAME} is missing; dict_japanese.dat must not ship without it", failures)
        return
    text = path.read_text(encoding="utf-8", errors="replace")
    absent = [term for term in MOZC_NOTICE_REQUIRED_TERMS if term.lower() not in text.lower()]
    if absent:
        fail(f"{MOZC_NOTICE_NAME} does not mention {', '.join(absent)}", failures)
        return
    print(f"ok   {MOZC_NOTICE_NAME} = {path.stat().st_size} bytes")


def check_checksums(failures: list[str]) -> None:
    path = OUT_DIR / "SHA256SUMS.txt"
    if not path.is_file():
        fail("SHA256SUMS.txt is missing", failures)
        return
    lines = [line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if len(lines) != EXPECTED_CHECKSUM_ENTRIES:
        fail(f"SHA256SUMS.txt lists {len(lines)} artifacts, expected {EXPECTED_CHECKSUM_ENTRIES}", failures)
    else:
        print(f"ok   SHA256SUMS.txt lists {len(lines)} artifacts")


def main() -> int:
    failures: list[str] = []
    check_row_counts(failures)
    check_quanpin(failures)
    check_japanese_model(failures)
    check_mozc_notice(failures)
    check_checksums(failures)

    if failures:
        print(f"\n{len(failures)} check(s) failed.")
        return 1
    print("\nAll dictionary checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
