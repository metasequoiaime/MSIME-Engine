"""
2. This is second step.

插入数据到数据库中。
"""

import os.path
import sqlite3
import string
from pathlib import Path

single_char_path = os.path.join(
    os.path.dirname(__file__), "../../../../cn/SingleCharsAllV1.txt"
)
single_char_whitelist_path = os.path.join(
    os.path.dirname(__file__), "../../../../cn/SingleCharWhitelist.txt"
)
basedict_part1_path = os.path.join(
    os.path.dirname(__file__), "../../../../cn/BaseDictAllV1Part1.txt"
)
basedict_part2_path = os.path.join(
    os.path.dirname(__file__), "../../../../cn/BaseDictAllV1Part2.txt"
)


repo_root = Path(__file__).resolve().parents[4]
import sys
sys.path.insert(0, str(repo_root))
from dictionary_format import pinyin_table, quanpin_tables
db_path = repo_root / "out" / "msime.db"
conn = sqlite3.connect(db_path)
cursor = conn.cursor()
insert_data_sql = """
insert into {} (
    key,
    jp,
    value,
    weight
) values (?, ?, ?, ?);
"""


def choose_tbl(pinyin_str: str) -> str:
    """
    pinyin_str: 分好词的全拼字符串，看有几个部分，就知道是几个字的词条了，不能使用汉字个数来划分，因为有些词条中不止包含汉字。
    """
    return pinyin_table(pinyin_str)


def load_single_char_whitelist(file_path: str) -> set[str]:
    with open(file_path, encoding="utf-8") as file:
        return {
            line.strip()
            for line in file
            if line.strip() and not line.startswith("#")
        }


def insert_lines_from_file_to_db_tbl(
    file_path: str, accepted_values: set[str] | None = None
):
    count = 0
    with open(file_path, "rb") as file:
        all_lines = file.readlines()
        for line in all_lines:
            cur_line = line.decode()
            if cur_line.startswith("#"):  # 跳过注释
                continue
            cur_line_list = cur_line.strip().split("\t")
            if accepted_values is not None and cur_line_list[0] not in accepted_values:
                continue
            if cur_line_list[1][0] not in string.ascii_lowercase:  # 滤掉一些如 ê 这样的
                continue
            cur_jp = "".join(pinyin[0] for pinyin in cur_line_list[1].split("'"))
            cur_line_tuple = tuple(
                [
                    cur_line_list[1],  # 拼音 key
                    cur_jp,  # 简拼 jp
                    cur_line_list[0],  # 汉字 value
                    cur_line_list[2],  # 权重 weight
                ]
            )
            count += 1
            cursor.execute(
                insert_data_sql.format(choose_tbl(cur_line_list[1])), cur_line_tuple
            )
    print(count)


# 插入单个汉字
single_char_whitelist = load_single_char_whitelist(single_char_whitelist_path)
insert_lines_from_file_to_db_tbl(single_char_path, single_char_whitelist)
# 插入词语
insert_lines_from_file_to_db_tbl(basedict_part1_path)
insert_lines_from_file_to_db_tbl(basedict_part2_path)

conn.commit()
conn.close()
