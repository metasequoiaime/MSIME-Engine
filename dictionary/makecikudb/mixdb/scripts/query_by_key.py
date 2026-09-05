"""按完整编码查询 quick_parases 表。"""

import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DB_PATH = REPO_ROOT / "out" / "msime.db"
TABLE_NAME = "quick_parases"


def query_by_key(key: str) -> None:
    key = key.strip().lower()
    if not DB_PATH.is_file():
        raise FileNotFoundError(f"Database does not exist: {DB_PATH}")

    with sqlite3.connect(DB_PATH) as conn:
        rows = conn.execute(
            f"SELECT \"key\", \"value\", \"weight\" FROM {TABLE_NAME} "
            f"WHERE \"key\" = ? ORDER BY \"weight\" DESC, \"value\"",
            (key,),
        ).fetchall()

    print(f"Exact key: {key}")
    print(f"Rows: {len(rows)}")
    for index, (row_key, value, weight) in enumerate(rows, start=1):
        print(f"{index:>3}. {row_key}\t{value}\t{weight}")


if __name__ == "__main__":
    query_key = "addr"
    query_by_key(query_key)
