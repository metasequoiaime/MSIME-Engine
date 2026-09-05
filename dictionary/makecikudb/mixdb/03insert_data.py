"""将 mix/quick_phrases.txt 导入快捷短语表。"""

import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATH = REPO_ROOT / "mix" / "quick_phrases.txt"
DB_PATH = REPO_ROOT / "out" / "msime.db"
TABLE_NAME = "quick_parases"

INSERT_SQL = f"""
INSERT INTO {TABLE_NAME} ("key", "value", "weight")
VALUES (?, ?, ?)
ON CONFLICT("key", "value") DO UPDATE SET
    "weight" = MAX("weight", excluded."weight")
"""


def parse_line(raw_line: str, line_number: int) -> tuple[str, str, int] | None:
    line = raw_line.rstrip("\r\n")
    if not line or line.lstrip().startswith("#"):
        return None

    columns = line.split("\t")
    if len(columns) != 3:
        print(f"Skipped line {line_number}: expected exactly 3 columns")
        return None

    key, value, weight_text = columns
    key = key.strip().lower()
    weight_text = weight_text.strip()

    if not key or not key.isascii() or not key.isalpha():
        print(f"Skipped line {line_number}: invalid key {key!r}")
        return None
    if not value:
        print(f"Skipped line {line_number}: value is empty")
        return None

    try:
        weight = int(weight_text)
    except ValueError:
        print(f"Skipped line {line_number}: invalid weight {weight_text!r}")
        return None

    if weight < 0:
        print(f"Skipped line {line_number}: weight must not be negative")
        return None

    return key, value, weight


def main() -> None:
    if not SOURCE_PATH.is_file():
        raise FileNotFoundError(f"Dictionary does not exist: {SOURCE_PATH}")
    if not DB_PATH.is_file():
        raise FileNotFoundError(f"Database does not exist: {DB_PATH}")

    imported_count = 0
    ignored_count = 0

    with sqlite3.connect(DB_PATH) as conn:
        with SOURCE_PATH.open("r", encoding="utf-8-sig", newline="") as source:
            for line_number, raw_line in enumerate(source, start=1):
                entry = parse_line(raw_line, line_number)
                if entry is None:
                    ignored_count += 1
                    continue
                conn.execute(INSERT_SQL, entry)
                imported_count += 1

    print(f"Imported {imported_count} rows from {SOURCE_PATH}")
    print(f"Ignored {ignored_count} blank, comment, or invalid rows")
    print(f"Database: {DB_PATH}")


if __name__ == "__main__":
    main()
