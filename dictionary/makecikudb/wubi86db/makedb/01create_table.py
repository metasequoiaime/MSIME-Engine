"""创建 86 五笔词表。"""

import sqlite3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
OUTPUT_DIR = REPO_ROOT / "out"
DB_PATH = OUTPUT_DIR / "msime.db"
TABLE_NAME = "wubi86"


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(f"DROP TABLE IF EXISTS {TABLE_NAME}")
        conn.execute(
            f"""
            CREATE TABLE {TABLE_NAME} (
                "key" TEXT NOT NULL,
                "value" TEXT NOT NULL,
                "weight" INTEGER NOT NULL DEFAULT 0,
                UNIQUE("key", "value")
            )
            """
        )

    print(f"Created table {TABLE_NAME} in {DB_PATH}")


if __name__ == "__main__":
    main()
