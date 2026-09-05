"""
2. This is second step.

Extract all rows from the xlsx table.

Insert all the data to to sqlite3 db `taiwan_chinese_dict` table.
"""
import os.path
import sqlite3

single_char_path = os.path.join(os.path.dirname(
    __file__), "../cn/SingleCharsAll_Ul_V1.txt")
basedict_part1_path = os.path.join(os.path.dirname(
    __file__), "../cn/BaseDictAllV1Part1_Ul_V1.txt")
basedict_part2_path = os.path.join(os.path.dirname(
    __file__), "../cn/BaseDictAllV1Part2_Ul_V1.txt")


db_path = os.path.join(os.path.dirname(__file__), "./out/flyciku.db")
conn = sqlite3.connect(db_path)
cursor = conn.cursor()
insert_data_sql = """
insert into xiaoheulpbtbl (
    key,
    value,
    weight
) values (?, ?, ?);
"""

count = 0
with open(single_char_path, "rb") as file:
    all_lines = file.readlines()
    for line in all_lines:
        cur_line_list = line.decode().strip().split("\t")
        cur_line_tuple = tuple([cur_line_list[1], cur_line_list[0], cur_line_list[2]])
        count += 1
        cursor.execute(insert_data_sql, cur_line_tuple)
print(count)

count = 0
with open(basedict_part1_path, "rb") as file:
    all_lines = file.readlines()
    for line in all_lines:
        cur_line_list = line.decode().strip().split("\t")
        cur_line_tuple = tuple([cur_line_list[1], cur_line_list[0], cur_line_list[2]])
        count += 1
        cursor.execute(insert_data_sql, cur_line_tuple)
print(count)

count = 0
with open(basedict_part2_path, "rb") as file:
    all_lines = file.readlines()
    for line in all_lines:
        cur_line_list = line.decode().strip().split("\t")
        cur_line_tuple = tuple([cur_line_list[1], cur_line_list[0], cur_line_list[2]])
        count += 1
        cursor.execute(insert_data_sql, cur_line_tuple)
print(count)

conn.commit()
conn.close()
