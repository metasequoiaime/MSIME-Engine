"""
4. This runs after the quanpin tables exist.

把 `custom/words.txt` 的人工词条并进全拼表。

这个文件从一开始就在仓库里,却没有任何东西读它:构建编排只接了 `custom/translations.txt`,
唯一会读 words.txt 的脚本在 `makecikudb/xiaoheshuangpindb/.../insert_custom_data.py`,那是一个
指向仓库外 `MyCustomDictForIme/words.txt`、依赖 `LOCALAPPDATA` 和 `win32gui` 的旧 Windows 脚本,
不在任何 stage 里。结果是这 39 条人工维护的词条从来没有进过发布词库。

权重只升不降:文件里的权重低于既有词条时保留既有的。人工文件的用途是补词和提词,历史上大部分
条目写的是占位的 1,按字面覆盖会把 `属于是` 这类同时存在于通用词库的词打到最底。
"""

from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT))
from dictionary_format import pinyin_table  # noqa: E402

DEFAULT_WORDS_PATH = REPO_ROOT / "custom" / "words.txt"
DEFAULT_DB_PATH = REPO_ROOT / "out" / "msime.db"

# 全拼 key:小写音节用 ' 分隔。分表规则由 dictionary_format 负责,这里只拦明显不是全拼的行。
PINYIN_PATTERN = re.compile(r"^[a-z]+(?:'[a-z]+)*$")


@dataclass(frozen=True)
class CustomWord:
    value: str
    key: str
    weight: int

    @property
    def jp(self) -> str:
        return "".join(syllable[0] for syllable in self.key.split("'"))

    @property
    def table(self) -> str:
        return pinyin_table(self.key)


def parse_custom_words(text: str) -> list[CustomWord]:
    """人工文件里一行写错就整条 stage 失败,不静默跳过 —— 39 行的文件不值得用沉默换容错。"""
    entries: dict[tuple[str, str], CustomWord] = {}
    for number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        fields = stripped.split("\t")
        if len(fields) != 3:
            raise ValueError(f"custom/words.txt:{number}: expected word, pinyin and weight: {line!r}")
        value, key, raw_weight = (field.strip() for field in fields)
        if not value:
            raise ValueError(f"custom/words.txt:{number}: the word is empty")
        if not PINYIN_PATTERN.match(key):
            raise ValueError(f"custom/words.txt:{number}: {key!r} is not quanpin separated by \"'\"")
        if not pinyin_table(key):
            raise ValueError(f"custom/words.txt:{number}: {key!r} maps to no quanpin table")
        try:
            weight = int(raw_weight)
        except ValueError as error:
            raise ValueError(f"custom/words.txt:{number}: weight {raw_weight!r} is not an integer") from error
        if weight < 1:
            raise ValueError(f"custom/words.txt:{number}: weight {weight} is below 1")
        entry = CustomWord(value=value, key=key, weight=weight)
        previous = entries.get((entry.key, entry.value))
        # 同一条写两次不是错误,取高的那个 —— 与下面「只升不降」是同一条规则。
        if previous is None or previous.weight < entry.weight:
            entries[(entry.key, entry.value)] = entry
    return list(entries.values())


def apply_custom_words(connection: sqlite3.Connection, entries: list[CustomWord]) -> dict[str, int]:
    counts = {"inserted": 0, "promoted": 0, "unchanged": 0}
    cursor = connection.cursor()
    for entry in entries:
        table = entry.table
        cursor.execute(
            f"select weight from {table} where key = ? and value = ?", (entry.key, entry.value)
        )
        row = cursor.fetchone()
        if row is None:
            cursor.execute(
                f"insert into {table} (key, jp, value, weight) values (?, ?, ?, ?)",
                (entry.key, entry.jp, entry.value, entry.weight),
            )
            counts["inserted"] += 1
        elif int(row[0]) < entry.weight:
            cursor.execute(
                f"update {table} set weight = ?, jp = ? where key = ? and value = ?",
                (entry.weight, entry.jp, entry.key, entry.value),
            )
            counts["promoted"] += 1
        else:
            counts["unchanged"] += 1
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--words", type=Path, default=DEFAULT_WORDS_PATH)
    parser.add_argument("--db", type=Path, default=DEFAULT_DB_PATH)
    arguments = parser.parse_args()
    entries = parse_custom_words(arguments.words.read_text(encoding="utf-8"))
    connection = sqlite3.connect(arguments.db)
    try:
        counts = apply_custom_words(connection, entries)
        connection.commit()
    finally:
        connection.close()
    print(
        f"custom words: {len(entries)} entries, {counts['inserted']} inserted, "
        f"{counts['promoted']} promoted, {counts['unchanged']} already at or above their weight"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
