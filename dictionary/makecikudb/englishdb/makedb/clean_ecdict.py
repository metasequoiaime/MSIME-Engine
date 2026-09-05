"""Build compact bidirectional candidate glosses from ECDICT.

The source CSV is intentionally left untouched.  This script intersects ECDICT
with the words already present in ``english.db``, turns the verbose ECDICT
translations into short candidate-window glosses, and writes two derived tables:

* ``en_zh_glosses``: English candidate -> short Chinese gloss
* ``zh_en_glosses``: Chinese candidate -> up to two ranked English words

The Chinese-to-English direction is deliberately conservative.  Only general
dictionary senses from English words with a corpus/core-vocabulary signal are
eligible for the reverse index.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sqlite3
from typing import Iterable

from apply_custom_translations import DEFAULT_SOURCE as DEFAULT_CUSTOM_TRANSLATIONS
from apply_custom_translations import apply_to_database, load_entries


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[2]
WORKSPACE_ROOT = REPOSITORY_ROOT.parent
DEFAULT_SOURCE = WORKSPACE_ROOT / "ReferenceProjects" / "ECDICT" / "ecdict.csv"
DEFAULT_ENGLISH_DB = REPOSITORY_ROOT / "out" / "english.db"
DEFAULT_CHINESE_DB = REPOSITORY_ROOT / "out" / "msime.db"
DEFAULT_REPORT = REPOSITORY_ROOT / "out" / "ecdict_cleaning_report.json"
DEFAULT_REVIEW = REPOSITORY_ROOT / "out" / "ecdict_cleaning_review.csv"

ASCII_WORD_RE = re.compile(r"^[a-z]+$")
CJK_TERM_RE = re.compile(r"^[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]+$")
LEADING_DOMAIN_RE = re.compile(r"^\s*(?:\[[^\]]+\]|【[^】]+】)\s*")
LEADING_POS_RE = re.compile(
    r"^\s*(?:(?:interj|abbr|modal|aux|adj|adv|prep|pron|conj|num|art|"
    r"sing|pref|suff|vt|vi|ad|pl|int|n|v|a)\.?\s*)+",
    re.IGNORECASE,
)
LEADING_PAREN_RE = re.compile(r"^\s*[（(][^）)]*[）)]\s*")
TRAILING_PAREN_RE = re.compile(r"\s*[（(][^）)]*[）)]\s*$")
INTERNAL_PAREN_RE = re.compile(r"[（(].*[）)]")
SPLIT_RE = re.compile(r"[,，;；、]+")
TRIM_PUNCTUATION = " \t\r\n.:：!?！？'\"“”‘’·•-—–_/\\"

# A reverse-index item containing one of these patterns is usually an
# explanation rather than a Chinese headword.
REVERSE_EXPLANATION_PREFIXES = (
    "表示",
    "用于",
    "用来",
    "用作",
    "指代",
    "即为",
    "一种",
    "一个",
    "某种",
    "某个",
)

TAG_BONUS = {
    "zk": 80_000,
    "gk": 75_000,
    "cet4": 70_000,
    "cet6": 60_000,
    "ky": 55_000,
    "ielts": 50_000,
    "toefl": 45_000,
    "gre": 30_000,
}

REVIEW_ENGLISH_WORDS = (
    "bank",
    "future",
    "function",
    "implement",
    "realize",
    "run",
    "set",
)
REVIEW_CHINESE_WORDS = (
    "未来",
    "功能",
    "实现",
    "银行",
    "运行",
    "设置",
)


@dataclass(frozen=True)
class GlossTerm:
    text: str
    line_index: int
    item_index: int
    domain_specific: bool
    part_of_speech: str
    reverse_domain_allowed: bool = False

    @property
    def reverse_eligible(self) -> bool:
        if (self.domain_specific and not self.reverse_domain_allowed) or not 2 <= len(self.text) <= 6:
            return False
        return not self.text.startswith(REVERSE_EXPLANATION_PREFIXES)


@dataclass
class CleanedEnglishEntry:
    english: str
    reverse_english: str
    quality: int
    terms: list[GlossTerm]
    reverse_terms: list[GlossTerm]
    chinese_gloss: str = ""


def parse_positive_int(value: str | None) -> int:
    try:
        parsed = int((value or "").strip())
    except ValueError:
        return 0
    return parsed if parsed > 0 else 0


def vocabulary_quality(row: dict[str, str]) -> int:
    collins = min(parse_positive_int(row.get("collins")), 5)
    oxford = 1 if parse_positive_int(row.get("oxford")) else 0
    tags = set((row.get("tag") or "").lower().split())
    tag_bonus = max((TAG_BONUS.get(tag, 0) for tag in tags), default=0)

    frq = parse_positive_int(row.get("frq"))
    bnc = parse_positive_int(row.get("bnc"))
    frq_bonus = max(0, 50_000 - min(frq, 50_000)) if frq else 0
    bnc_bonus = max(0, 25_000 - min(bnc, 50_000) // 2) if bnc else 0
    return collins * 100_000 + oxford * 80_000 + tag_bonus + frq_bonus + bnc_bonus


def _strip_parenthetical_qualifiers(value: str) -> str | None:
    """Remove leading/trailing qualifiers but reject embedded explanations."""

    previous = None
    while value != previous:
        previous = value
        value = LEADING_PAREN_RE.sub("", value)
        value = TRAILING_PAREN_RE.sub("", value)
    if INTERNAL_PAREN_RE.search(value):
        return None
    return value


def extract_gloss_terms(translation: str) -> list[GlossTerm]:
    terms: list[GlossTerm] = []
    seen: set[str] = set()

    # ECDICT stores line breaks as the two literal characters ``\n`` in many
    # rows rather than as embedded CSV newlines.
    translation = translation.replace("\\n", "\n")
    for line_index, raw_line in enumerate(translation.splitlines()):
        line = raw_line.strip()
        if not line:
            continue

        domain_specific = False
        reverse_domain_allowed = False
        while (domain_match := LEADING_DOMAIN_RE.match(line)) is not None:
            domain_specific = True
            label = domain_match.group(0)
            reverse_domain_allowed = reverse_domain_allowed or "计" in label or "网络" in label
            line = LEADING_DOMAIN_RE.sub("", line, count=1)
        pos_match = LEADING_POS_RE.match(line)
        part_of_speech = pos_match.group(0).strip().rstrip(".").lower() if pos_match else ""
        line = LEADING_POS_RE.sub("", line, count=1).strip()

        for item_index, raw_item in enumerate(SPLIT_RE.split(line)):
            item = raw_item.strip(TRIM_PUNCTUATION)
            item = _strip_parenthetical_qualifiers(item)
            if item is None:
                continue
            item = item.strip(TRIM_PUNCTUATION)
            if not item or len(item) > 8 or not CJK_TERM_RE.fullmatch(item):
                continue
            if item in seen:
                continue
            seen.add(item)
            terms.append(
                GlossTerm(
                    item,
                    line_index,
                    item_index,
                    domain_specific,
                    part_of_speech,
                    reverse_domain_allowed,
                )
            )

    return terms


def choose_chinese_gloss(
    terms: Iterable[GlossTerm],
    chinese_term_weights: dict[str, int] | None = None,
    limit: int = 2,
) -> str:
    weights = chinese_term_weights or {}
    term_texts = {term.text for term in terms}
    filtered_terms = [
        term
        for term in terms
        if not (term.text.endswith("的") and term.text[:-1] in term_texts)
    ]
    ranked = sorted(
        filtered_terms,
        key=lambda term: (
            term.domain_specific,
            -weights.get(term.text, 0),
            term.line_index,
            term.item_index,
            len(term.text),
            term.text,
        ),
    )
    return "；".join(term.text for term in ranked[:limit])


def reverse_pair_score(quality: int, english: str, term: GlossTerm) -> int:
    position_bonus = max(0, 30_000 - term.line_index * 2_000 - term.item_index * 500)
    return quality + position_bonus - len(english) * 10


def canonical_reverse_word(
    row: dict[str, str],
    english: str,
    english_candidates: set[str],
) -> str:
    for exchange_item in (row.get("exchange") or "").split("/"):
        if not exchange_item.startswith("0:"):
            continue
        lemma = exchange_item[2:].strip().lower()
        if lemma in english_candidates and ASCII_WORD_RE.fullmatch(lemma):
            return lemma
    return english


def load_english_candidates(database: sqlite3.Connection) -> set[str]:
    return {
        str(row[0]).lower()
        for row in database.execute("SELECT DISTINCT word FROM english_words")
        if row[0] is not None and ASCII_WORD_RE.fullmatch(str(row[0]).lower())
    }


def scan_source(
    source_path: Path,
    english_candidates: set[str],
) -> tuple[dict[str, CleanedEnglishEntry], dict[str, dict[str, int]], dict[str, int]]:
    english_entries: dict[str, CleanedEnglishEntry] = {}
    reverse_candidates: dict[str, dict[str, int]] = {}
    stats = {
        "source_rows": 0,
        "candidate_word_matches": 0,
        "matched_rows_with_translation": 0,
        "matched_rows_without_clean_gloss": 0,
        "reverse_pairs_considered": 0,
    }

    with source_path.open("r", encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        required = {"word", "translation", "collins", "oxford", "tag", "bnc", "frq"}
        missing = required.difference(reader.fieldnames or ())
        if missing:
            raise ValueError(f"ECDICT CSV is missing columns: {sorted(missing)}")

        for row in reader:
            stats["source_rows"] += 1
            english = (row.get("word") or "").strip().lower()
            if english not in english_candidates:
                continue
            stats["candidate_word_matches"] += 1

            translation = (row.get("translation") or "").strip()
            if not translation:
                continue
            stats["matched_rows_with_translation"] += 1

            terms = extract_gloss_terms(translation)
            if not terms:
                stats["matched_rows_without_clean_gloss"] += 1
                continue

            quality = vocabulary_quality(row)
            reverse_terms = [term for term in terms if quality > 0 and term.reverse_eligible]
            reverse_english = canonical_reverse_word(row, english, english_candidates)
            entry = CleanedEnglishEntry(english, reverse_english, quality, terms, reverse_terms)
            previous = english_entries.get(english)
            if previous is None or (entry.quality, len(entry.terms)) > (previous.quality, len(previous.terms)):
                english_entries[english] = entry

    # Build the reverse candidates only after duplicate English rows have been
    # resolved, otherwise a duplicated source row can distort the ranking.
    for entry in english_entries.values():
        for term in entry.reverse_terms:
            stats["reverse_pairs_considered"] += 1
            candidates = reverse_candidates.setdefault(term.text, {})
            score = reverse_pair_score(entry.quality, entry.reverse_english, term)
            previous_score = candidates.get(entry.reverse_english)
            if previous_score is None or score > previous_score:
                candidates[entry.reverse_english] = score

    return english_entries, reverse_candidates, stats


def load_chinese_term_weights(database_path: Path, terms: set[str]) -> dict[str, int]:
    """Scan the pinyin tables for matching candidates and their best weight."""

    if not database_path.exists() or not terms:
        return {}

    weights: dict[str, int] = {}
    with sqlite3.connect(database_path) as database:
        table_names = [
            str(row[0])
            for row in database.execute(
                "SELECT name FROM sqlite_master "
                "WHERE type='table' AND name GLOB 'tbl_*_[a-z]' ORDER BY name"
            )
            if re.fullmatch(r"tbl_(?:[1-7]|others)_[a-z]", str(row[0]))
        ]
        for table_name in table_names:
            # Table names come exclusively from sqlite_master and are restricted
            # by the GLOB above; values remain data, never SQL identifiers.
            for value, weight in database.execute(f'SELECT value,weight FROM "{table_name}"'):
                if value in terms:
                    term = str(value)
                    weights[term] = max(weights.get(term, 0), int(weight or 0))
    return weights


def apply_chinese_glosses(
    english_entries: dict[str, CleanedEnglishEntry],
    chinese_term_weights: dict[str, int],
) -> None:
    for entry in english_entries.values():
        entry.chinese_gloss = choose_chinese_gloss(entry.terms, chinese_term_weights)


def finalize_reverse_glosses(
    reverse_candidates: dict[str, dict[str, int]],
    allowed_chinese_terms: set[str] | None,
    limit: int = 2,
) -> dict[str, str]:
    result: dict[str, str] = {}
    for chinese, candidates in reverse_candidates.items():
        if allowed_chinese_terms is not None and chinese not in allowed_chinese_terms:
            continue
        ranked = sorted(candidates.items(), key=lambda item: (-item[1], len(item[0]), item[0]))
        if ranked:
            top_score = ranked[0][1]
            selected = [
                english
                for english, score in ranked
                if score * 100 >= top_score * 45
            ][:limit]
            result[chinese] = "; ".join(selected)
    return result


def replace_gloss_tables(
    database: sqlite3.Connection,
    english_entries: dict[str, CleanedEnglishEntry],
    reverse_glosses: dict[str, str],
) -> None:
    database.execute("BEGIN IMMEDIATE")
    try:
        database.executescript(
            """
            DROP TABLE IF EXISTS en_zh_glosses_new;
            DROP TABLE IF EXISTS zh_en_glosses_new;
            CREATE TABLE en_zh_glosses_new (
                english TEXT COLLATE BINARY PRIMARY KEY,
                chinese_gloss TEXT NOT NULL
            ) WITHOUT ROWID;
            CREATE TABLE zh_en_glosses_new (
                chinese TEXT COLLATE BINARY PRIMARY KEY,
                english_gloss TEXT NOT NULL
            ) WITHOUT ROWID;
            """
        )
        database.executemany(
            "INSERT INTO en_zh_glosses_new(english,chinese_gloss) VALUES(?1,?2)",
            ((english, entry.chinese_gloss) for english, entry in sorted(english_entries.items())),
        )
        database.executemany(
            "INSERT INTO zh_en_glosses_new(chinese,english_gloss) VALUES(?1,?2)",
            sorted(reverse_glosses.items()),
        )
        database.executescript(
            """
            DROP TABLE IF EXISTS en_zh_glosses;
            ALTER TABLE en_zh_glosses_new RENAME TO en_zh_glosses;
            DROP TABLE IF EXISTS zh_en_glosses;
            ALTER TABLE zh_en_glosses_new RENAME TO zh_en_glosses;
            PRAGMA user_version=3;
            """
        )
        database.commit()
    except Exception:
        database.rollback()
        raise


def write_review_csv(
    path: Path,
    english_entries: dict[str, CleanedEnglishEntry],
    reverse_glosses: dict[str, str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(("direction", "candidate", "gloss"))
        for english in REVIEW_ENGLISH_WORDS:
            entry = english_entries.get(english)
            writer.writerow(("en_to_zh", english, entry.chinese_gloss if entry else ""))
        for chinese in REVIEW_CHINESE_WORDS:
            writer.writerow(("zh_to_en", chinese, reverse_glosses.get(chinese, "")))


def source_git_revision(source_path: Path) -> str:
    head_path = source_path.parent / ".git" / "HEAD"
    if not head_path.exists():
        return "unknown"
    head = head_path.read_text(encoding="utf-8").strip()
    if not head.startswith("ref: "):
        return head
    ref_path = source_path.parent / ".git" / head[5:]
    return ref_path.read_text(encoding="utf-8").strip() if ref_path.exists() else head[5:]


def write_report(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--english-db", type=Path, default=DEFAULT_ENGLISH_DB)
    parser.add_argument("--chinese-db", type=Path, default=DEFAULT_CHINESE_DB)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--review", type=Path, default=DEFAULT_REVIEW)
    parser.add_argument(
        "--keep-non-candidates",
        action="store_true",
        help="Do not restrict reverse glosses to words present in msime.db.",
    )
    parser.add_argument(
        "--custom-translations",
        type=Path,
        default=DEFAULT_CUSTOM_TRANSLATIONS,
        help="Small override list applied after the ECDICT tables are rebuilt.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not args.source.exists():
        raise FileNotFoundError(args.source)
    if not args.english_db.exists():
        raise FileNotFoundError(args.english_db)

    with sqlite3.connect(args.english_db) as english_database:
        english_candidates = load_english_candidates(english_database)
        english_entries, reverse_candidates, stats = scan_source(args.source, english_candidates)

        all_clean_terms = {term.text for entry in english_entries.values() for term in entry.terms}
        chinese_term_weights = load_chinese_term_weights(args.chinese_db, all_clean_terms)
        apply_chinese_glosses(english_entries, chinese_term_weights)

        allowed_terms: set[str] | None = None
        if not args.keep_non_candidates:
            allowed_terms = set(chinese_term_weights)
        reverse_glosses = finalize_reverse_glosses(reverse_candidates, allowed_terms)
        replace_gloss_tables(english_database, english_entries, reverse_glosses)
        custom_applied = 0
        if args.custom_translations.exists():
            custom_applied = apply_to_database(
                english_database, load_entries(args.custom_translations)
            )["total"]
            english_database.commit()
        integrity = english_database.execute("PRAGMA integrity_check").fetchone()[0]

    report: dict[str, object] = {
        "source": str(args.source.resolve()),
        "source_revision": source_git_revision(args.source),
        "english_database": str(args.english_db.resolve()),
        "chinese_database": str(args.chinese_db.resolve()) if args.chinese_db.exists() else None,
        **stats,
        "english_candidate_words": len(english_candidates),
        "en_zh_glosses": len(english_entries),
        "reverse_terms_before_candidate_filter": len(reverse_candidates),
        "clean_terms_present_in_chinese_dictionary": len(chinese_term_weights),
        "reverse_terms_present_in_chinese_dictionary": len(allowed_terms & reverse_candidates.keys())
        if allowed_terms is not None
        else None,
        "zh_en_glosses": len(reverse_glosses),
        "custom_translations": custom_applied,
        "integrity_check": integrity,
        "review_english": {
            word: english_entries[word].chinese_gloss
            for word in REVIEW_ENGLISH_WORDS
            if word in english_entries
        },
        "review_chinese": {
            word: reverse_glosses[word]
            for word in REVIEW_CHINESE_WORDS
            if word in reverse_glosses
        },
    }
    write_report(args.report, report)
    write_review_csv(args.review, english_entries, reverse_glosses)

    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
