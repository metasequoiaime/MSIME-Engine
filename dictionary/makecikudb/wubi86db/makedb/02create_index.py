"""为 86 五笔词表创建精确查询索引。"""

import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DB_PATH = REPO_ROOT / "out" / "msime.db"
TABLE_NAME = "wubi86"
INDEX_NAME = "idx_wubi86_key_weight"


def main() -> None:
    if not DB_PATH.is_file():
        raise FileNotFoundError(f"Database does not exist: {DB_PATH}")

    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(f"DROP INDEX IF EXISTS {INDEX_NAME}")
        conn.execute(
            f"CREATE INDEX {INDEX_NAME} "
            f"ON {TABLE_NAME}(\"key\", \"weight\" DESC)"
        )

    print(f"Created index {INDEX_NAME} in {DB_PATH}")


if __name__ == "__main__":
    main()
