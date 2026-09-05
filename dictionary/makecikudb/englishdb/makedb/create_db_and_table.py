"""1. Create the English prefix-candidate database and table in ./out."""

from pathlib import Path
import sqlite3


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[2]
OUTPUT_DIR = REPOSITORY_ROOT / "out"
DB_PATH = OUTPUT_DIR / "english.db"


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("DROP TABLE IF EXISTS english_words")
        conn.execute(
            """
            CREATE TABLE english_words (
                word TEXT COLLATE BINARY NOT NULL,
                display TEXT NOT NULL,
                weight INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (word, display)
            ) WITHOUT ROWID
            """
        )
    print(f"Created: {DB_PATH}")


if __name__ == "__main__":
    main()
