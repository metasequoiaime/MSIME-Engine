"""
2. This is second step.

Insert all the data to to sqlite3 db `imeciku.db` and table `quanpintbl`.
"""
import os.path
import sqlite3

single_char_path = os.path.join(os.path.dirname(
    __file__), "../../../../cn/SingleCharsAllV1.txt")
basedict_part1_path = os.path.join(os.path.dirname(
    __file__), "../../../../cn/BaseDictAllV1Part1.txt")
basedict_part2_path = os.path.join(os.path.dirname(
    __file__), "../../../../cn/BaseDictAllV1Part2.txt")


db_path = os.path.join(os.path.dirname(__file__), "./out/imeciku.db")
conn = sqlite3.connect(db_path)
cursor = conn.cursor()
insert_data_sql = """
insert into quanpintbl (
    key,
    jp,
    value,
    weight
) values (?, ?, ?, ?);
"""

def extract_jp(pinyin: str) -> str:
    if "'" not in pinyin:
        return pinyin[0]
    else:
        return "'".join(s[0] for s in pinyin.split("'"))

def insert_lines_from_file_to_db_tbl(file_path: str):
    count = 0
    with open(file_path, "rb") as file:
        all_lines = file.readlines()
        for line in all_lines:
            cur_line = line.decode()
            if (cur_line.startswith("#")): # 跳过注释
                continue
            cur_line_list = cur_line.strip().split("\t")
            cur_line_tuple = tuple([cur_line_list[1], extract_jp(cur_line_list[1]), cur_line_list[0], cur_line_list[2]])
            count += 1
            cursor.execute(insert_data_sql, cur_line_tuple)
    print(count)

# 插入单个汉字
insert_lines_from_file_to_db_tbl(single_char_path)
insert_lines_from_file_to_db_tbl(basedict_part1_path)
insert_lines_from_file_to_db_tbl(basedict_part2_path)

conn.commit()
conn.close()
