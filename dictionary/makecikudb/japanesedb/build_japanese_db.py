"""Import the jp_sela Rime dictionary into the shared SQLite candidate database.

This table contains direct kana/character candidates.  The immutable sentence
model (dict_japanese.dat) is intentionally a separate build artifact.
"""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path


DICT_REPO_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SOURCE = WORKSPACE_ROOT / "ReferenceProjects" / "rime-jp_sela" / "jp_sela.dict.yaml"
DEFAULT_DATABASE = DICT_REPO_ROOT / "out" / "msime.db"


def read_entries(source: Path) -> list[tuple[str, str, int]]:
    entries: list[tuple[str, str, int]] = []
    in_body = False
    with source.open("r", encoding="utf-8-sig") as stream:
        for line_number, raw_line in enumerate(stream, start=1):
            line = raw_line.rstrip("\r\n")
            if not in_body:
                if line.strip() == "...":
                    in_body = True
                continue
            if not line or line.lstrip().startswith("#"):
                continue
            columns = line.split("\t")
            if len(columns) < 2:
                continue
            value, code = columns[0].strip(), columns[1].strip()
            if not value or not code or any(ord(ch) > 127 for ch in code):
                continue
            if len(columns) >= 3:
                try:
                    weight = int(columns[2])
                except ValueError:
                    weight = 1_000_000 - line_number
            else:
                weight = 1_000_000 - line_number
            entries.append((code, value, weight))
    return entries


def build(source: Path, database: Path) -> int:
    if not source.is_file():
        raise FileNotFoundError(f"Japanese source dictionary not found: {source}")
    if not database.is_file():
        raise FileNotFoundError(f"Candidate database not found: {database}")

    entries = read_entries(source)
    if not entries:
        raise RuntimeError(f"No Japanese entries parsed from: {source}")

    with sqlite3.connect(database) as connection:
        connection.executescript(
            """
            CREATE TABLE IF NOT EXISTS japanese_lexicon (
                code TEXT NOT NULL,
                value TEXT NOT NULL,
                weight INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (code, value)
            );
            DELETE FROM japanese_lexicon;
            """
        )
        connection.executemany(
            "INSERT INTO japanese_lexicon(code, value, weight) VALUES (?, ?, ?) "
            "ON CONFLICT(code, value) DO UPDATE SET weight=MAX(weight, excluded.weight)",
            entries,
        )
        connection.execute(
            "CREATE INDEX IF NOT EXISTS idx_japanese_lexicon_code_weight "
            "ON japanese_lexicon(code, weight DESC)"
        )
    return len(entries)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--database", type=Path, default=DEFAULT_DATABASE)
    args = parser.parse_args()
    count = build(args.source.resolve(), args.database.resolve())
    print(f"Imported {count} Japanese entries")
    print(f"Source: {args.source.resolve()}")
    print(f"Database: {args.database.resolve()}")


if __name__ == "__main__":
    main()
