"""校验快捷短语表并优化数据库。"""

import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DB_PATH = REPO_ROOT / "out" / "msime.db"
TABLE_NAME = "quick_parases"


def main() -> None:
    if not DB_PATH.is_file():
        raise FileNotFoundError(f"Database does not exist: {DB_PATH}")

    with sqlite3.connect(DB_PATH) as conn:
        integrity = conn.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise RuntimeError(f"SQLite integrity check failed: {integrity}")

        total_count = conn.execute(f"SELECT COUNT(*) FROM {TABLE_NAME}").fetchone()[0]
        distinct_count = conn.execute(
            f"SELECT COUNT(*) FROM "
            f"(SELECT 1 FROM {TABLE_NAME} GROUP BY \"key\", \"value\")"
        ).fetchone()[0]
        if total_count == 0 or total_count != distinct_count:
            raise RuntimeError(
                f"Unexpected row counts: rows={total_count}, distinct_entries={distinct_count}"
            )

        rows = conn.execute(
            f"SELECT \"key\", \"value\", \"weight\" FROM {TABLE_NAME} "
            f"ORDER BY \"key\", \"weight\" DESC"
        ).fetchall()
        conn.execute("ANALYZE")
        conn.execute("PRAGMA optimize")

    print(f"Integrity: {integrity}")
    print(f"Rows: {total_count}")
    for key, value, weight in rows:
        print(f"  {key}\t{value}\t{weight}")


if __name__ == "__main__":
    main()
