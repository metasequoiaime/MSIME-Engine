# -*- coding: utf-8 -*-
"""Build the symbol_catalog table inside MetasequoiaImeDict/out/others.db.

Source: MetasequoiaImeDict/symbols/piliapp_symbols.txt (PiliApp Unicode symbols,
grouped into 51 site categories). Empty categories are skipped. HTML entities
such as &dollar; are decoded to the actual characters.

The emoji panel maps these into a smaller set of parent tabs (like the emoji
category bar) and keeps the original PiliApp names as section headings.
"""
from collections import OrderedDict
from html import unescape
from pathlib import Path
import os
import re
import sqlite3

from pypinyin import lazy_pinyin

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
SYMBOLS_DIR = REPO_ROOT / "symbols"
OUT_DIR = REPO_ROOT / "out"
DB_PATH = OUT_DIR / "others.db"

CATEGORY_LINE = re.compile(r"^# \[piliapp/symbol/([^/\]]+)/\]\s*(.+)\s*$")

# Display names for the original PiliApp categories (emoji-panel UI is English).
CATEGORY_EN = {
    "activity": "People and activity",
    "animals": "Animals",
    "arrow": "Arrows",
    "asterisk": "Asterisks",
    "brackets": "Brackets",
    "braille": "Braille",
    "bullet-point": "Bullet points",
    "business": "Business",
    "card-suit": "Card suits",
    "chess": "Chess",
    "circle": "Circles",
    "confidential": "Block elements",
    "cross": "Crosses",
    "culture": "Religion and culture",
    "currency": "Currency",
    "dice": "Dice",
    "eye": "Eyes",
    "flower": "Flowers",
    "fraction": "Fractions",
    "heart": "Hearts",
    "totem": "Totems",
    "triangle": "Triangles",
    "gender": "Gender",
    "greek": "Greek letters",
    "kana": "Japanese kana",
    "korean": "Korean",
    "latin-extended": "Latin extended",
    "latin": "Latin letters",
    "line": "Lines",
    "mashup": "Mashup",
    "math": "Math",
    "menu": "Menu",
    "misc": "Miscellaneous",
    "monochrome": "Monochrome",
    "music": "Music",
    "number": "Numbers",
    "other-shapes": "Other shapes",
    "pi": "Pi",
    "pilcrow": "Pilcrow",
    "punctuation": "Punctuation",
    "quotation-mark": "Quotation marks",
    "random-lines": "Random lines",
    "square": "Squares",
    "star": "Stars",
    "subscript-superscript": "Super/subscripts",
    "tech": "Technical",
    "tick": "Check marks",
    "unit": "Units",
    "weather": "Weather",
    "x-mark": "X marks",
    "zodiac": "Zodiac",
}

# Parent tabs shown in the emoji panel, in navigation order. Slug order inside
# a tab is also heading order, and the first symbol of the first slug is the
# tab icon.
PARENT_TABS: list[tuple[str, list[str]]] = [
    ("Stars and shapes", ["star", "asterisk", "circle", "triangle", "square", "other-shapes", "bullet-point"]),
    ("Arrows and lines", ["arrow", "line", "random-lines"]),
    ("Punctuation", ["punctuation", "brackets", "quotation-mark", "pilcrow", "tick", "x-mark"]),
    ("Math", ["math", "number", "fraction", "pi", "subscript-superscript"]),
    ("Currency", ["currency", "business", "unit"]),
    ("Hearts", ["heart", "eye", "monochrome"]),
    ("Letters", ["latin", "latin-extended", "greek", "kana", "braille", "korean"]),
    ("Games", ["card-suit", "chess", "dice"]),
    ("Culture", ["culture", "cross", "zodiac", "gender", "totem"]),
    ("Animals and nature", ["animals", "flower", "weather"]),
    ("People and activity", ["activity"]),
    ("More", ["music", "tech", "menu", "misc", "confidential", "mashup"]),
]

SLUG_TO_PARENT = {
    slug: parent for parent, slugs in PARENT_TABS for slug in slugs
}


def is_cjk(text: str) -> bool:
    return any("\u4e00" <= ch <= "\u9fff" for ch in text)


def keyword_pinyin(text: str) -> list[str]:
    if not is_cjk(text):
        return []
    full = "".join(lazy_pinyin(text))
    initials = "".join(s[0] for s in lazy_pinyin(text) if s)
    keys = []
    if full:
        keys.append(full.lower())
    if initials and initials.lower() != (full or "").lower():
        keys.append(initials.lower())
    return keys


def parse_piliapp(path: Path) -> OrderedDict[str, tuple[str, list[str]]]:
    """slug -> (chinese_title, symbols)."""
    categories: OrderedDict[str, tuple[str, list[str]]] = OrderedDict()
    slug = ""
    title = ""
    symbols: list[str] = []
    seen_in_category: set[str] = set()

    def flush() -> None:
        if slug:
            categories[slug] = (title, list(symbols))

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        match = CATEGORY_LINE.match(line)
        if match:
            flush()
            slug = match.group(1).strip()
            title = match.group(2).strip()
            symbols = []
            seen_in_category = set()
            continue
        if line.startswith("#"):
            continue
        symbol = unescape(line).strip()
        if not symbol or symbol in seen_in_category:
            continue
        seen_in_category.add(symbol)
        symbols.append(symbol)
    flush()
    return categories


def build_rows(categories: OrderedDict[str, tuple[str, list[str]]]) -> list[tuple[str, str, str, int, str]]:
    rows: list[tuple[str, str, str, int, str]] = []
    order = 0
    used_slugs: set[str] = set()
    for parent, slugs in PARENT_TABS:
        for slug in slugs:
            used_slugs.add(slug)
            chinese, symbols = categories.get(slug, ("", []))
            if not symbols:
                continue
            category_en = CATEGORY_EN.get(slug, slug.replace("-", " ").title())
            keyword_parts = [category_en, chinese, slug.replace("-", " "), parent]
            keyword_parts.extend(keyword_pinyin(chinese))
            keywords = " ".join(part for part in keyword_parts if part)
            for symbol in symbols:
                rows.append((symbol, category_en, parent, order, keywords))
                order += 1

    leftovers = [slug for slug in categories if slug not in used_slugs and categories[slug][1]]
    if leftovers:
        raise ValueError(f"Unmapped PiliApp categories: {leftovers}")
    return rows


def create_and_insert(rows: list[tuple[str, str, str, int, str]]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("DROP TABLE IF EXISTS symbol_catalog")
        conn.execute(
            """
            CREATE TABLE symbol_catalog (
                symbol TEXT NOT NULL,
                category TEXT NOT NULL,
                parent_category TEXT NOT NULL,
                sort_order INTEGER NOT NULL,
                keywords TEXT NOT NULL,
                PRIMARY KEY (symbol, category)
            ) WITHOUT ROWID
            """
        )
        conn.execute(
            "CREATE INDEX idx_symbol_catalog_parent_order ON symbol_catalog(parent_category, sort_order)"
        )
        conn.executemany(
            "INSERT INTO symbol_catalog (symbol, category, parent_category, sort_order, keywords) VALUES (?, ?, ?, ?, ?)",
            rows,
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
    source = SYMBOLS_DIR / "piliapp_symbols.txt"
    categories = parse_piliapp(source)
    rows = build_rows(categories)
    create_and_insert(rows)
    copy_to_appdata()

    parents: OrderedDict[str, int] = OrderedDict()
    cats: OrderedDict[str, int] = OrderedDict()
    for _symbol, category, parent, _order, _keywords in rows:
        parents[parent] = parents.get(parent, 0) + 1
        cats[category] = cats.get(category, 0) + 1

    skipped = [f"{slug} ({title})" for slug, (title, symbols) in categories.items() if not symbols]
    print(f"Created: {DB_PATH}")
    print(f"symbol_catalog entries: {len(rows)}")
    print(f"parent tabs: {len(parents)}")
    print(f"categories: {len(cats)}")
    for name, count in parents.items():
        print(f"  {name}: {count}")
    if skipped:
        print("skipped empty: " + ", ".join(skipped))


if __name__ == "__main__":
    main()
