"""
1. This is first step.

Create a db, and create a table for 全拼
Columns all comes from the original xlsx table title.

The output db path is ./out/imeciku.db
"""
import sqlite3
import os.path

output_path = os.path.join(os.path.dirname(__file__), "./out")
if not os.path.exists(output_path):
    os.makedirs(output_path)
db_path = os.path.join(output_path, "./imeciku.db")
# if there is no db, then connect will create one automatically
conn = sqlite3.connect(db_path)
cursor = conn.cursor()

delete_table_sql = """
drop table if exists 'quanpintbl';
"""

cursor.execute(delete_table_sql)

# sql statement to create a table
create_table_sql = '''
create table if not exists quanpintbl (
   "key" text, -- 双拼拼音
   "jp" text, -- 拼音的简拼
   "value" text, -- 对应的汉字或者词组
   "weight" integer default 0 -- 权重
);
'''

# execute sql statement
cursor.execute(create_table_sql)
# commit changes
conn.commit()
# close connection
conn.close()
