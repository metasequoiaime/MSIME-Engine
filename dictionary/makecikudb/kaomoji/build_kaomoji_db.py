# -*- coding: utf-8 -*-
"""Build the kaomoji lookup table inside MetasequoiaImeDict/out/others.db.

A single table maps every keyword of every kaomoji to its full-pinyin and
abbreviated-pinyin forms (one row per keyword):
    kaomoji(pinyin TEXT, jianpin TEXT, kaomoji TEXT, sort_order INTEGER)

- Chinese keyword ("害羞")        -> pinyin "haixiu", jianpin "hx"
- pinyin-code keyword ("zai xiang") -> pinyin "zaixiang", jianpin "zx"
- English keyword ("kiss")        -> pinyin "kiss", jianpin ""

The M-mode query is a prefix range scan over both pinyin and jianpin columns.
"""
from collections import defaultdict
from pathlib import Path
import os
import re
import sqlite3

from pypinyin import lazy_pinyin

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
KAOMOJI_DIR = REPO_ROOT / "kaomoji"
OUT_DIR = REPO_ROOT / "out"
DB_PATH = OUT_DIR / "others.db"

# English function words that carry no search meaning (see build_emoji_db.py).
EN_STOPWORDS = {
    "a", "an", "the",
    "of", "in", "at", "by", "for", "with", "to", "from", "into", "onto", "upon", "via",
    "and", "or", "but", "nor", "yet", "so",
    "as", "if", "then", "than", "that", "this", "these", "those",
    "be", "is", "are", "was", "were", "am", "been", "being",
    "it", "its", "all", "any", "both", "each", "few", "more", "most", "other", "some", "such",
    "only", "own", "same", "too", "very", "can", "will", "just", "should", "would", "could",
    "do", "does", "did", "have", "has", "had", "there", "here", "which", "who", "whom", "whose",
}


def is_cjk(text: str) -> bool:
    return any("\u4e00" <= ch <= "\u9fff" for ch in text)


def is_pinyin_phrase(keyword: str) -> bool:
    """ASCII keyword that is already a space-separated pinyin code ("zai xiang")."""
    return " " in keyword and bool(re.fullmatch(r"[a-z ]+", keyword.lower()))


def keyword_to_pinyin(keyword: str) -> str | None:
    if not is_cjk(keyword):
        return None
    return "".join(lazy_pinyin(keyword))


def keyword_to_initials(keyword: str) -> str | None:
    if not is_cjk(keyword):
        return None
    return "".join(s[0] for s in lazy_pinyin(keyword) if s)


def keyword_columns(keyword: str) -> tuple[str, str]:
    """Return the (pinyin, jianpin) columns for one keyword; ("", "") to drop."""
    if is_cjk(keyword):
        pinyin = keyword_to_pinyin(keyword)
        jianpin = keyword_to_initials(keyword)
        if pinyin:
            pinyin = pinyin.lower()
        if jianpin:
            jianpin = jianpin.lower()
        if jianpin == pinyin:
            jianpin = ""
        return (pinyin or "", jianpin or "")
    if is_pinyin_phrase(keyword):
        syllables = keyword.lower().split()
        full = "".join(syllables)
        initials = "".join(s[0] for s in syllables)
        return full, (initials if initials != full else "")
    word = keyword.lower().strip(".,:;!?()[]{}<>-/\"'`")
    if word and word.isalpha() and word not in EN_STOPWORDS:
        return word, ""
    return "", ""


def load_keyword_map(path: Path) -> dict[str, list[str]]:
    mapping: dict[str, list[str]] = defaultdict(list)
    seen: dict[str, set[str]] = defaultdict(set)
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        keyword, kaomoji = stripped.split("\t", 1)
        keyword = keyword.strip()
        kaomoji = kaomoji.strip()
        if not keyword or not kaomoji or keyword in seen[kaomoji]:
            continue
        seen[kaomoji].add(keyword)
        mapping[kaomoji].append(keyword)
    return mapping


def build_catalog_rows(mapping: dict[str, list[str]]) -> list[tuple[str, int, str]]:
    """One row per unique kaomoji for the emoji-panel catalog (browse + keyword search)."""
    rows: list[tuple[str, int, str]] = []
    order = 0
    for kaomoji, keywords in mapping.items():
        rows.append((kaomoji, order, " ".join(keywords)))
        order += 1
    return rows


def build_rows(mapping: dict[str, list[str]]) -> list[tuple[str, str, str, int]]:
    rows: list[tuple[str, str, str, int]] = []
    seen: set[tuple[str, str, str]] = set()
    order = 0
    for kaomoji in mapping:  # dict preserves insertion order = file order
        for keyword in mapping[kaomoji]:
            pinyin, jianpin = keyword_columns(keyword)
            if not pinyin:
                continue
            key = (pinyin, jianpin, kaomoji)
            if key in seen:
                continue
            seen.add(key)
            rows.append((pinyin, jianpin, kaomoji, order))
        order += 1
    return rows


def create_and_insert(rows: list[tuple[str, str, str, int]],
                      catalog_rows: list[tuple[str, int, str]]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("DROP TABLE IF EXISTS kaomoji")
        conn.execute("DROP TABLE IF EXISTS kaomoji_pinyin")
        conn.execute("DROP INDEX IF EXISTS idx_kaomoji_jianpin")
        conn.execute("DROP TABLE IF EXISTS kaomoji_catalog")
        conn.execute(
            """
            CREATE TABLE kaomoji (
                pinyin TEXT NOT NULL,
                jianpin TEXT NOT NULL,
                kaomoji TEXT NOT NULL,
                sort_order INTEGER NOT NULL,
                PRIMARY KEY (pinyin, jianpin, kaomoji)
            ) WITHOUT ROWID
            """
        )
        conn.execute("CREATE INDEX idx_kaomoji_jianpin ON kaomoji(jianpin)")
        conn.executemany(
            "INSERT INTO kaomoji (pinyin, jianpin, kaomoji, sort_order) VALUES (?, ?, ?, ?)",
            rows,
        )

        conn.execute(
            """
            CREATE TABLE kaomoji_catalog (
                kaomoji TEXT NOT NULL,
                sort_order INTEGER NOT NULL,
                keywords TEXT NOT NULL,
                PRIMARY KEY (kaomoji)
            ) WITHOUT ROWID
            """
        )
        conn.executemany(
            "INSERT INTO kaomoji_catalog (kaomoji, sort_order, keywords) VALUES (?, ?, ?)",
            catalog_rows,
        )
        conn.execute("ANALYZE")
        conn.commit()


def copy_to_appdata() -> None:
    local = os.environ.get("LOCALAPPDATA")
    if not local:
        return
    dest_dir = Path(local) / "metasequoiaime"
    if not dest_dir.is_dir():
        return
    dest = dest_dir / "others.db"
    dest.write_bytes(DB_PATH.read_bytes())
    print(f"Copied: {dest}")


def main() -> None:
    mapping = load_keyword_map(KAOMOJI_DIR / "kaomoji.txt")
    rows = build_rows(mapping)
    catalog_rows = build_catalog_rows(mapping)
    create_and_insert(rows, catalog_rows)
    copy_to_appdata()
    print(f"Created: {DB_PATH}")
    print(f"kaomoji rows (per keyword): {len(rows)}")
    print(f"kaomoji entries: {len(mapping)}")
    print(f"kaomoji_catalog entries: {len(catalog_rows)}")


if __name__ == "__main__":
    main()
