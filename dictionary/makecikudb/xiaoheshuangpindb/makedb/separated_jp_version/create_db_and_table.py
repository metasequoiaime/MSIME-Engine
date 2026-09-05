"""
1. This is first step.

Create a db, and create a table for 小鹤双拼
Columns all comes from the original xlsx table title.

The output db path is ./out/flyciku.db
"""
import sqlite3
import os.path
import string

output_path = os.path.join(os.path.dirname(__file__), "./out")
if not os.path.exists(output_path):
    os.makedirs(output_path)
db_path = os.path.join(output_path, "./cutted_flyciku_with_jp.db")
# if there is no db, then connect will create one automatically
conn = sqlite3.connect(db_path)
cursor = conn.cursor()

delete_table_sql = """
drop table if exists {};
"""
# sql statement to create a table
create_table_sql = """
create table if not exists {} (
   "key" text, -- 双拼拼音
   "jp" text, -- 双拼简拼
   "value" text, -- 对应的汉字或者词组
   "weight" integer default 0 -- 权重
);
"""

base_tbl = "tbl_{}_{}"  # e.g. tbl_3_a means 三个汉字的词条，并且双拼拼音以 a 开头
for i in range(8):
    for c in string.ascii_lowercase:
        cur_tbl = base_tbl.format(i + 1 if i < 7 else "others", c)
        cursor.execute(delete_table_sql.format(cur_tbl))
        cursor.execute(create_table_sql.format(cur_tbl))

# commit changes
conn.commit()
# close connection
conn.close()
