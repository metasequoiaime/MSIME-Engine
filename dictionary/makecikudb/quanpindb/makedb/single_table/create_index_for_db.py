"""
3. This is third step.

Create index for key and value.
"""
import os.path
import sqlite3


db_path = os.path.join(os.path.dirname(__file__), "./out/imeciku.db")
conn = sqlite3.connect(db_path)
cursor = conn.cursor()

create_index_key_sql = """
create index index_key on quanpintbl(key);
"""

create_index_jp_sql = """
create index index_jp on quanpintbl(jp);
"""

create_index_value_sql = """
create index index_value on quanpintbl(value);
"""

cursor.execute(create_index_key_sql)
cursor.execute(create_index_jp_sql)
cursor.execute(create_index_value_sql)

conn.commit()
conn.close()
