# -*- coding: utf-8 -*-
"""Build the emoji table inside MetasequoiaImeDict/out/others.db."""

import argparse
from collections import defaultdict, OrderedDict
from pathlib import Path
import os
import sqlite3

from pypinyin import lazy_pinyin

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
EMOJI_DIR = REPO_ROOT / "emoji"
OUT_DIR = REPO_ROOT / "out"
DB_PATH = OUT_DIR / "others.db"

CATEGORY_TITLES = {
    "Smileys & Emotion": "Smileys and emotion",
    "People & Body": "People and body",
    "Animals & Nature": "Animals and nature",
    "Food & Drink": "Food and drink",
    "Travel & Places": "Travel and places",
    "Activities": "Activities",
    "Objects": "Objects",
    "Symbols": "Symbols",
    "Flags": "Flags",
}

TEST_LINE = (
    r"^(?P<codes>[0-9A-Fa-f ]+?)\s*;\s*(?P<status>fully-qualified|component)\s*"
    r"#\s*(?P<emoji>\S+)\s+E[0-9.]+\s+(?P<name>.+)$"
)


VS16 = "\ufe0f"


def strip_vs(text: str) -> str:
    return text.replace(VS16, "")


def load_keyword_map(path: Path) -> dict[str, list[str]]:
    mapping: dict[str, list[str]] = defaultdict(list)
    seen: dict[str, set[str]] = defaultdict(set)
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        keyword, emoji = stripped.split("\t", 1)
        keyword = keyword.strip()
        emoji = emoji.strip()
        if not keyword or not emoji or keyword in seen[emoji]:
            continue
        seen[emoji].add(keyword)
        mapping[emoji].append(keyword)
    return mapping


def keywords_for(emoji: str, mapping: dict[str, list[str]], stripped_index: dict[str, list[str]]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for keyword in mapping.get(emoji, []) + stripped_index.get(strip_vs(emoji), []):
        if keyword not in seen:
            seen.add(keyword)
            result.append(keyword)
    return result


def index_by_stripped(mapping: dict[str, list[str]]) -> dict[str, list[str]]:
    index: dict[str, list[str]] = defaultdict(list)
    seen: dict[str, set[str]] = defaultdict(set)
    for emoji, keywords in mapping.items():
        key = strip_vs(emoji)
        for keyword in keywords:
            if keyword not in seen[key]:
                seen[key].add(keyword)
                index[key].append(keyword)
    return index


def is_cjk(text: str) -> bool:
    return any("\u4e00" <= ch <= "\u9fff" for ch in text)


def keyword_to_pinyin(keyword: str) -> str | None:
    """Full pinyin for a Chinese keyword; None for tokens without CJK."""
    if not is_cjk(keyword):
        return None
    return "".join(lazy_pinyin(keyword))


def keyword_to_initials(keyword: str) -> str | None:
    """Abbreviated pinyin (initial of each syllable) for a Chinese keyword.

    "笑脸" -> "xl", "大熊猫" -> "dxm".
    """
    if not is_cjk(keyword):
        return None
    return "".join(s[0] for s in lazy_pinyin(keyword) if s)


def pinyin_for_keywords(keywords: list[str]) -> str:
    """Space-joined full pinyin of the Chinese keywords in `keywords`."""
    parts = [p for p in (keyword_to_pinyin(k) for k in keywords) if p]
    return " ".join(parts)


def load_catalog(emoji_test_path: Path) -> OrderedDict[str, tuple[str, int]]:
    import re

    pattern = re.compile(TEST_LINE)
    catalog: OrderedDict[str, tuple[str, int]] = OrderedDict()
    group = ""
    order = 0
    for line in emoji_test_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# group:"):
            group = line.split(":", 1)[1].strip()
            continue
        match = pattern.match(line)
        if not match:
            continue
        name = match.group("name").strip()
        emoji = match.group("emoji")
        status = match.group("status")
        if status == "component":
            continue
        if "skin tone" in name.lower():
            continue
        title = CATEGORY_TITLES.get(group)
        if not title:
            continue
        if emoji in catalog:
            continue
        catalog[emoji] = (title, order)
        order += 1
    return catalog


def write_catalog(catalog: OrderedDict[str, tuple[str, int]], path: Path) -> None:
    lines = ["# emoji<TAB>category<TAB>sort_order"]
    for emoji, (category, order) in catalog.items():
        lines.append(f"{emoji}\t{category}\t{order}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def read_catalog(path: Path) -> OrderedDict[str, tuple[str, int]]:
    catalog: OrderedDict[str, tuple[str, int]] = OrderedDict()
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        emoji, category, order = stripped.split("\t")
        catalog[emoji] = (category, int(order))
    return catalog


# English function words that carry no emoji-search meaning. Directional or
# symbolic words that ARE useful in emoji context (up/down/no/not/on/off/out)
# are deliberately excluded.
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


def search_keys(merged: list[str]) -> list[str]:
    """All E-mode lookup keys for a merged keyword list.

    Chinese keywords contribute their full pinyin and abbreviated pinyin
    (initials); English keywords contribute their individual words (lowercased),
    skipping function-word stopwords. For example ["扭曲", "鱼眼", "distorted face"]
    -> ["niuqu", "nq", "yuyan", "yy", "distorted", "face"].
    """
    keys: list[str] = []
    for keyword in merged:
        if is_cjk(keyword):
            pinyin = keyword_to_pinyin(keyword)
            if pinyin:
                keys.append(pinyin)
            initials = keyword_to_initials(keyword)
            if initials and initials != pinyin:
                keys.append(initials)
        else:
            for word in keyword.lower().split():
                word = word.strip(".,:;!?()[]{}<>-/\"'`")
                if word and word.isalpha() and word not in EN_STOPWORDS:
                    keys.append(word)
    return keys


def build_rows(
    catalog: OrderedDict[str, tuple[str, int]],
    zh: dict[str, list[str]],
    en: dict[str, list[str]],
) -> tuple[list[tuple[str, str, int, str, str]], list[list[str]]]:
    zh_index = index_by_stripped(zh)
    en_index = index_by_stripped(en)
    rows: list[tuple[str, str, int, str, str]] = []
    keys_list: list[list[str]] = []
    known = {strip_vs(emoji) for emoji in catalog}
    for emoji, (category, order) in catalog.items():
        merged: list[str] = []
        seen: set[str] = set()
        for keyword in keywords_for(emoji, zh, zh_index) + keywords_for(emoji, en, en_index):
            if keyword not in seen:
                seen.add(keyword)
                merged.append(keyword)
        rows.append((emoji, category, order, " ".join(merged), pinyin_for_keywords(merged)))
        keys_list.append(search_keys(merged))

    extra_order = max((order for _, order in catalog.values()), default=-1) + 1
    extras = []
    for source in (zh, en):
        for emoji in source:
            key = strip_vs(emoji)
            if key in known:
                continue
            known.add(key)
            merged: list[str] = []
            seen: set[str] = set()
            for keyword in keywords_for(emoji, zh, zh_index) + keywords_for(emoji, en, en_index):
                if keyword not in seen:
                    seen.add(keyword)
                    merged.append(keyword)
            extras.append((emoji, "Symbols", extra_order, " ".join(merged), pinyin_for_keywords(merged)))
            keys_list.append(search_keys(merged))
            extra_order += 1
    rows.extend(extras)
    return rows, keys_list


def build_pinyin_rows(rows: list[tuple[str, str, int, str, str]],
                      keys_list: list[list[str]]) -> list[tuple[str, str, int]]:
    """One row per search key (Chinese pinyin or English word), for E-mode prefix lookup.

    `key` is a single token (e.g. "niuqu", "yuyan", "laugh") so that a typed
    shuangpin/quanpin/English code can prefix-match any keyword of an emoji.
    """
    pinyin_rows: list[tuple[str, str, int]] = []
    seen: set[tuple[str, str]] = set()
    for (emoji, _category, sort_order, _keywords, _pinyin), keys in zip(rows, keys_list):
        for key in keys:
            if not key or (key, emoji) in seen:
                continue
            seen.add((key, emoji))
            pinyin_rows.append((key, emoji, sort_order))
    return pinyin_rows


def create_and_insert(rows: list[tuple[str, str, int, str, str]],
                      pinyin_rows: list[tuple[str, str, int]]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("DROP TABLE IF EXISTS emoji")
        conn.execute("DROP INDEX IF EXISTS idx_emoji_category_order")
        conn.execute(
            """
            CREATE TABLE emoji (
                emoji TEXT NOT NULL,
                category TEXT NOT NULL,
                sort_order INTEGER NOT NULL,
                keywords TEXT NOT NULL,
                pinyin TEXT NOT NULL,
                PRIMARY KEY (emoji)
            ) WITHOUT ROWID
            """
        )
        conn.execute(
            "CREATE INDEX idx_emoji_category_order ON emoji(category, sort_order)"
        )
        conn.executemany(
            "INSERT INTO emoji (emoji, category, sort_order, keywords, pinyin) VALUES (?, ?, ?, ?, ?)",
            rows,
        )

        conn.execute("DROP TABLE IF EXISTS emoji_pinyin")
        conn.execute(
            """
            CREATE TABLE emoji_pinyin (
                key TEXT NOT NULL,
                emoji TEXT NOT NULL,
                sort_order INTEGER NOT NULL,
                PRIMARY KEY (key, emoji)
            ) WITHOUT ROWID
            """
        )
        conn.executemany(
            "INSERT INTO emoji_pinyin (key, emoji, sort_order) VALUES (?, ?, ?)",
            pinyin_rows,
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
    leftover = dest_dir / "emoji.db"
    if leftover.exists():
        leftover.unlink()
        print(f"Removed leftover: {leftover}")


def find_emoji_test(override: Path | None = None) -> Path | None:
    """Locate Unicode's emoji-test.txt, used to regenerate emoji_catalog.txt.

    The file is not committed; download it from unicode.org when the catalog needs refreshing.
    Without it the build reads the committed emoji_catalog.txt instead, which is the normal path.
    """
    candidates = [override, EMOJI_DIR / "emoji-test.txt"]
    for path in candidates:
        if path is not None and path.is_file():
            return path
    return None


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--emoji-test",
        type=Path,
        default=None,
        help="path to Unicode's emoji-test.txt; regenerates emoji_catalog.txt when given",
    )
    args = parser.parse_args()

    catalog_path = EMOJI_DIR / "emoji_catalog.txt"
    emoji_test = find_emoji_test(args.emoji_test)
    if emoji_test:
        catalog = load_catalog(emoji_test)
        write_catalog(catalog, catalog_path)
        print(f"Wrote catalog: {catalog_path} ({len(catalog)} emojis)")
    elif catalog_path.is_file():
        catalog = read_catalog(catalog_path)
        print(f"Loaded catalog: {catalog_path} ({len(catalog)} emojis)")
    else:
        raise FileNotFoundError("Need emoji-test.txt or emoji_catalog.txt")

    zh = load_keyword_map(EMOJI_DIR / "emoji.txt")
    en = load_keyword_map(EMOJI_DIR / "emoji_en.txt")
    rows, keys_list = build_rows(catalog, zh, en)
    pinyin_rows = build_pinyin_rows(rows, keys_list)
    create_and_insert(rows, pinyin_rows)
    leftover_out = OUT_DIR / "emoji.db"
    if leftover_out.exists():
        leftover_out.unlink()
        print(f"Removed leftover: {leftover_out}")
    copy_to_appdata()

    categories: dict[str, int] = OrderedDict()
    for _, category, _, _, _ in rows:
        categories[category] = categories.get(category, 0) + 1
    print(f"Created: {DB_PATH}")
    print(f"rows: {len(rows)}")
    print(f"emoji_pinyin keys: {len(pinyin_rows)}")
    for name, count in categories.items():
        print(f"  {name}: {count}")


if __name__ == "__main__":
    main()
