"""Overlay a small custom translation list onto english.db gloss tables.

The large ECDICT-derived tables stay untouched as a source.  This script only
INSERT OR REPLACES the keys listed in ``MetasequoiaImeCustomDict/translations.txt``.
"""

from __future__ import annotations

import argparse
import os
from dataclasses import dataclass
from pathlib import Path
import shutil
import sqlite3


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[2]
DEFAULT_SOURCE = REPOSITORY_ROOT / "MetasequoiaImeCustomDict" / "translations.txt"
DEFAULT_ENGLISH_DB = REPOSITORY_ROOT / "out" / "english.db"
DEFAULT_SIDECAR = REPOSITORY_ROOT / "out" / "custom_translations.txt"
CJK_RE_START = 0x3400


@dataclass(frozen=True)
class CustomTranslation:
    chinese_to_english: bool
    source: str
    gloss: str


def is_chinese_source(text: str) -> bool:
    return any(ord(char) >= CJK_RE_START for char in text)


def parse_translations(text: str) -> list[CustomTranslation]:
    entries: list[CustomTranslation] = []
    if text.startswith("\ufeff"):
        text = text[1:]
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 2:
            raise ValueError(f"line {line_number}: expected source<TAB>gloss, got {raw_line!r}")
        source = fields[0].strip()
        gloss = fields[1].strip()
        if not source or not gloss:
            raise ValueError(f"line {line_number}: empty source or gloss")
        entries.append(CustomTranslation(is_chinese_source(source), source, gloss))
    return entries


def load_entries(path: Path) -> list[CustomTranslation]:
    return parse_translations(path.read_text(encoding="utf-8"))


def apply_to_database(database: sqlite3.Connection, entries: list[CustomTranslation]) -> dict[str, int]:
    zh_en = 0
    en_zh = 0
    database.executescript(
        """
        CREATE TABLE IF NOT EXISTS en_zh_glosses(
            english TEXT COLLATE BINARY PRIMARY KEY,
            chinese_gloss TEXT NOT NULL
        ) WITHOUT ROWID;
        CREATE TABLE IF NOT EXISTS zh_en_glosses(
            chinese TEXT COLLATE BINARY PRIMARY KEY,
            english_gloss TEXT NOT NULL
        ) WITHOUT ROWID;
        """
    )
    for entry in entries:
        if entry.chinese_to_english:
            database.execute(
                "INSERT OR REPLACE INTO zh_en_glosses(chinese,english_gloss) VALUES(?1,?2)",
                (entry.source, entry.gloss),
            )
            zh_en += 1
        else:
            database.execute(
                "INSERT OR REPLACE INTO en_zh_glosses(english,chinese_gloss) VALUES(?1,?2)",
                (entry.source, entry.gloss),
            )
            en_zh += 1
    return {"zh_en": zh_en, "en_zh": en_zh, "total": len(entries)}


def default_app_data_english_db() -> Path | None:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        return None
    path = Path(local_app_data) / "metasequoiaime" / "english.db"
    return path if path.exists() else None


def default_app_data_sidecar() -> Path | None:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        return None
    directory = Path(local_app_data) / "metasequoiaime"
    return directory / "custom_translations.txt" if directory.exists() else None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--english-db", type=Path, default=DEFAULT_ENGLISH_DB)
    parser.add_argument("--sidecar", type=Path, default=DEFAULT_SIDECAR)
    parser.add_argument(
        "--also-app-data",
        action="store_true",
        default=True,
        help="Also overlay the installed IME english.db and copy the sidecar.",
    )
    parser.add_argument("--no-app-data", action="store_true")
    return parser.parse_args()


def overlay_database(db_path: Path, entries: list[CustomTranslation]) -> dict[str, int]:
    if not db_path.exists():
        raise FileNotFoundError(db_path)
    database = sqlite3.connect(db_path)
    try:
        database.execute("PRAGMA busy_timeout=2000")
        counts = apply_to_database(database, entries)
        database.commit()
    finally:
        database.close()
    return counts


def main() -> None:
    args = parse_args()
    if not args.source.exists():
        raise FileNotFoundError(args.source)
    entries = load_entries(args.source)
    args.sidecar.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.source, args.sidecar)

    targets: list[Path] = []
    if args.english_db.exists():
        overlay_database(args.english_db, entries)
        targets.append(args.english_db)

    if not args.no_app_data and args.also_app_data:
        app_db = default_app_data_english_db()
        if app_db is not None:
            overlay_database(app_db, entries)
            targets.append(app_db)
        app_sidecar = default_app_data_sidecar()
        if app_sidecar is not None:
            shutil.copyfile(args.source, app_sidecar)

    if not targets:
        raise FileNotFoundError(args.english_db)

    print(f"applied {len(entries)} custom translations to {len(targets)} database(s)")


if __name__ == "__main__":
    main()
