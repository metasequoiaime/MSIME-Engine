"""显示 quick_parases 表中的全部快捷短语。"""

import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DB_PATH = REPO_ROOT / "out" / "msime.db"
TABLE_NAME = "quick_parases"


def main() -> None:
    if not DB_PATH.is_file():
        raise FileNotFoundError(f"Database does not exist: {DB_PATH}")

    with sqlite3.connect(DB_PATH) as conn:
        rows = conn.execute(
            f"SELECT \"key\", \"value\", \"weight\" FROM {TABLE_NAME} "
            f"ORDER BY \"key\", \"weight\" DESC, \"value\""
        ).fetchall()

    print(f"Database: {DB_PATH}")
    print(f"Table: {TABLE_NAME}")
    print(f"Rows: {len(rows)}")
    print("-" * 80)
    for index, (key, value, weight) in enumerate(rows, start=1):
        print(f"{index:>3}. {key}\t{value}\t{weight}")


if __name__ == "__main__":
    main()
