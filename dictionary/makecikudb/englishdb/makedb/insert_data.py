"""2. Merge OALDPE and BaseDictIceEn words into the English database."""

from collections import defaultdict
from pathlib import Path
import sqlite3


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[2]
OALDPE_WORDS_PATH = REPOSITORY_ROOT / "en" / "oaldpe_words.txt"
BASE_DICT_PATH = REPOSITORY_ROOT / "en" / "BaseDictIceEn.txt"
DB_PATH = REPOSITORY_ROOT / "out" / "english.db"
BATCH_SIZE = 10_000


def is_ascii_word(value: str) -> bool:
    return bool(value) and value.isascii() and value.isalpha()


def load_oaldpe_words() -> set[str]:
    words: set[str] = set()
    with OALDPE_WORDS_PATH.open("r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, start=1):
            word = line.strip()
            if not is_ascii_word(word) or not word.islower():
                raise ValueError(
                    f"{OALDPE_WORDS_PATH}:{line_number}: expected a lowercase ASCII word, got {word!r}"
                )
            if word in words:
                raise ValueError(
                    f"{OALDPE_WORDS_PATH}:{line_number}: duplicate word {word!r}"
                )
            words.add(word)

    if not words:
        raise ValueError(f"{OALDPE_WORDS_PATH}: file is empty")
    return words


def parse_base_dict_display(line: str, line_number: int) -> str | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return None

    fields = stripped.split()
    if len(fields) < 2:
        raise ValueError(
            f"{BASE_DICT_PATH}:{line_number}: expected display input-code [weight]"
        )
    if len(fields) >= 3 and fields[-1].isdigit():
        fields.pop()

    # The final field is the source dictionary's input code. English prefix
    # lookup uses the display word itself, so codes such as "Jan" and "a11y"
    # are deliberately ignored.
    fields.pop()
    display = " ".join(fields)
    if not is_ascii_word(display):
        return None
    return display


def load_base_dict_words() -> dict[str, str]:
    displays_by_word: dict[str, set[str]] = defaultdict(set)
    with BASE_DICT_PATH.open("r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, start=1):
            display = parse_base_dict_display(line, line_number)
            if display is not None:
                displays_by_word[display.lower()].add(display)

    if not displays_by_word:
        raise ValueError(f"{BASE_DICT_PATH}: no pure English words found")

    # Preserve unambiguous casing such as "Aaliyah". When both a common-word
    # and proper-name casing exist, use lowercase instead of guessing.
    return {
        word: next(iter(displays)) if len(displays) == 1 else word
        for word, displays in displays_by_word.items()
    }


def build_rows(
    oaldpe_words: set[str], base_words: dict[str, str]
) -> list[tuple[str, str]]:
    all_words = oaldpe_words | base_words.keys()
    return [
        (word, base_words.get(word, word))
        for word in sorted(all_words)
    ]


def insert_rows(conn: sqlite3.Connection, rows: list[tuple[str, str]]) -> None:
    sql = "INSERT INTO english_words(word, display) VALUES (?, ?)"
    for start in range(0, len(rows), BATCH_SIZE):
        conn.executemany(sql, rows[start : start + BATCH_SIZE])


def main() -> None:
    if not DB_PATH.exists():
        raise FileNotFoundError(
            f"Database does not exist: {DB_PATH}. Run create_db_and_table.py first."
        )

    oaldpe_words = load_oaldpe_words()
    base_words = load_base_dict_words()
    rows = build_rows(oaldpe_words, base_words)
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("PRAGMA journal_mode = WAL")
        conn.execute("PRAGMA synchronous = NORMAL")
        conn.execute("BEGIN IMMEDIATE")
        insert_rows(conn, rows)

    print(f"OALDPE words: {len(oaldpe_words)}")
    print(f"BaseDict pure English words: {len(base_words)}")
    print(f"Overlapping words: {len(oaldpe_words & base_words.keys())}")
    print(f"Inserted unique words: {len(rows)}")


if __name__ == "__main__":
    main()
