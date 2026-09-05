"""按编码前缀查询 quick_parases 表。"""

import argparse
import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DB_PATH = REPO_ROOT / "out" / "msime.db"
TABLE_NAME = "quick_parases"


def main() -> None:
    parser = argparse.ArgumentParser(description="按编码前缀查询快捷短语")
    parser.add_argument("prefix", help="要查询的编码前缀，例如 a 或 n")
    args = parser.parse_args()
    prefix = args.prefix.strip().lower()

    if not DB_PATH.is_file():
        raise FileNotFoundError(f"Database does not exist: {DB_PATH}")

    with sqlite3.connect(DB_PATH) as conn:
        rows = conn.execute(
            f"SELECT \"key\", \"value\", \"weight\" FROM {TABLE_NAME} "
            f"WHERE \"key\" >= ? AND \"key\" < ? "
            f"ORDER BY \"key\", \"weight\" DESC, \"value\"",
            (prefix, prefix + "\U0010ffff"),
        ).fetchall()

    print(f"Key prefix: {prefix}")
    print(f"Rows: {len(rows)}")
    for index, (key, value, weight) in enumerate(rows, start=1):
        print(f"{index:>3}. {key}\t{value}\t{weight}")


if __name__ == "__main__":
    main()
