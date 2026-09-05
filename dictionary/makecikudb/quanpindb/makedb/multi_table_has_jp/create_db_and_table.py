"""
1. This is first step.

创建一个数据库，并且创建多个表格用于存储全拼的词条

数据库输出路径是：仓库根目录/out/msime.db
"""

import sqlite3
from pathlib import Path
import string

repo_root = Path(__file__).resolve().parents[4]
import sys
sys.path.insert(0, str(repo_root))
from dictionary_format import pinyin_table, quanpin_tables
output_path = repo_root / "out"
output_path.mkdir(parents=True, exist_ok=True)
db_path = output_path / "msime.db"
# if there is no db, then connect will create one automatically
conn = sqlite3.connect(db_path)
cursor = conn.cursor()

delete_table_sql = """
drop table if exists {};
"""
# sql statement to create a table
create_table_sql = """
create table if not exists {} (
   "key" text, -- 全拼拼音
   "jp" text, -- 全拼简拼
   "value" text, -- 对应的汉字或者词组
   "weight" integer default 0 -- 权重
);
"""

for cur_tbl in quanpin_tables():
    cursor.execute(delete_table_sql.format(cur_tbl))
    cursor.execute(create_table_sql.format(cur_tbl))

# commit changes
conn.commit()
# close connection
conn.close()
